"""Connect to the hydration sensor over BLE and decode its records (packet v2).

Usage:

    python ble_decode.py              # print records until Ctrl-C
    python ble_decode.py --count 20   # stop and disconnect after 20 records
    python ble_decode.py --hex <hex>  # decode one 17-byte record from the device shell, no radio
    python ble_decode.py --selftest   # check parser, decoder and reassembly, no radio

Live mode needs bleak, declared in pyproject.toml: run `uv run ble_decode.py` from this folder.
The other modes use only the standard library.

Where the layout comes from:

    The field table and version come from src/ble/packet.h (the X() rows of PACKET_FIELDS and
    PACKET_VERSION). Flag bits, reject names and status names come from src/hs/hs_types.h when
    that file exists. Packet v2 has no flags or status field, so the file is optional. This
    script has no field list of its own. The drawings below help the reader, but the header is
    the source of truth.

One fragment = one BLE notification (little-endian):

    byte   0      1      2      3      4        5        6 ...
         +------+------+------+------+--------+--------+--------------+
         |  session_id |   sequence  | index  | count  | record bytes |
         +------+------+------+------+--------+--------+--------------+
          u16           u16           u8       u8

    The receiver joins the fragments with the same (session_id, sequence) in index order.

One record, packet v2, 17 bytes (little-endian):

    byte   0      1      2      3      4      5      6      7      8
         +------+------+------+------+------+------+------+------+------+
         | ver  |  session_id |        timestamp_s        |  sequence   |
         +------+------+------+------+------+------+------+------+------+
          u8     u16           u32                         u16

    byte   9      10     11     12     13     14     15     16
         +------+------+------+------+------+------+------+------+
         | temp_centi_c| a_rms_mm_s2 | battery_mv  | record_crc16|
         +------+------+------+------+------+------+------+------+
          i16           i16           i16           u16

    record_crc16 is CRC-16/CCITT-FALSE (seed 0xFFFF) over every byte before it.
"""

import argparse
import asyncio
import binascii
import dataclasses
import pathlib
import re
import struct
import sys

SRC_DIR = pathlib.Path(__file__).resolve().parent.parent / "src"
RECORD_HEADER = SRC_DIR / "ble/packet.h"
TYPES_HEADER = SRC_DIR / "hs/hs_types.h"

DEVICE_NAME = "Hydration"  # CONFIG_BT_DEVICE_NAME in prj.conf
RECORD_UUID = "3f6c0002-8a4e-4b5d-9c2a-6e1f0b7d4a21"  # RECORD_UUID_VAL in ble_service.c
REPAIR_WAIT_S = 60  # time to wait for a new PAIRING window after a host unpair
REPAIR_RETRY_S = 2  # pause between pairing attempts in that time

FRAGMENT_HEADER = struct.Struct("<HHBB")

# Wire type in the header -> struct format character (all little-endian).
WIRE_FORMAT = {"u8": "B", "u16": "H", "i16": "h", "u32": "I"}

# One row of the field table: X(name, wire type, scale, "unit"), with an optional trailing "\".
# The scale may be negative, have a fraction, or have an exponent.
FIELD_ROW = re.compile(
    r'^\s*X\((\w+),\s*(\w+),\s*(-?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?),\s*"([^"]*)"\)\s*\\?\s*$'
)

CRC_SEED = 0xFFFF  # PACKET_CRC_INIT

# Values that mean "not available": any i16 field at its minimum, and these u16 fields at 0xFFFF.
NA_I16 = -32768
NA_U16 = 0xFFFF
NA_U16_FIELDS = ("R0_deci_ohm", "Rinf_deci_ohm", "fit_error_bp", "optional_raw_index")

# Parts of a flags field, when the record has one.
REJECT_MASK = 0xF  # reject reason: 4 bits at reject_shift
MODE_MASK = 0x3  # measurement mode: 2 bits at mode_shift
MODE_NAMES = ("MANUAL", "TIMER", "CONTINUOUS")
BUILD_TAGS = (("SIM", "BUILD_AFE_SIM"), ("ORACLE", "BUILD_ORACLE"))  # (tag, flag name)

SEQUENCE_MASK = 0xFFFF  # sequence is u16 and wraps at 65536
HALF_SEQUENCE_RANGE = 0x8000  # a step this large or larger counts as going backwards


@dataclasses.dataclass(frozen=True)
class Field:
    """One row of the field table in packet.h.

    Attributes:
        name (str): Member name in struct packet.
        wire_type (str): Wire type, for example "u16". It is a key of WIRE_FORMAT.
        scale (float): Real value = raw integer * scale.
        unit (str): Unit of the real value. Empty when the value has no unit.
    """

    name: str
    wire_type: str
    scale: float
    unit: str


@dataclasses.dataclass
class HeaderNames:
    """Flag, reject and status names from hs_types.h.

    All dicts are empty and both shifts are None when hs_types.h does not exist.

    Attributes:
        flag_bits (dict[int, str]): Bit number to flag name.
        reject_names (dict[int, str]): Reject value to reject name.
        status_names (dict[int, str]): Status value to status name.
        reject_shift (int | None): Bit position of the 4-bit reject reason in the flags field.
        mode_shift (int | None): Bit position of the 2-bit mode in the flags field.
    """

    flag_bits: dict
    reject_names: dict
    status_names: dict
    reject_shift: int | None
    mode_shift: int | None

def load_table(path=RECORD_HEADER):
    """Read the record version and the field table from the firmware header.

    Args:
        path (pathlib.Path, optional): Header to read. Defaults to RECORD_HEADER.

    Returns:
        tuple[int, list[Field]]: PACKET_VERSION and the fields in wire order.

    Raises:
        OSError: The header cannot be read.
        SystemExit: The header has no PACKET_VERSION or no X() rows.
    """
    text = path.read_text()
    version = re.search(r"#define PACKET_VERSION (\d+)", text)
    fields = []
    for line in text.splitlines():
        row = FIELD_ROW.match(line)
        if row:
            name, wire_type, scale, unit = row.groups()
            fields.append(Field(name, wire_type, float(scale), unit))
    if not version or not fields:
        sys.exit(f"cannot parse version or field table from {path}")
    return int(version.group(1)), fields


def parse_enum(text, tag, prefix):
    """Read the members of one C enum.

    Only members with an explicit "= <number>" count.

    Args:
        text (str): Contents of the C header.
        tag (str): Enum tag, for example "hs_reject".
        prefix (str): Prefix to remove from each member name, for example "HS_REJECT_".

    Returns:
        dict[int, str]: Member value to member name without the prefix. Empty when text has
            no such enum.
    """
    body = re.search(rf"enum {tag} \{{(.*?)\}};", text, re.S)
    if not body:
        return {}
    members = {}
    for name, value in re.findall(rf"{prefix}(\w+)\s*=\s*(\d+)", body.group(1)):
        members[int(value)] = name
    return members


def parse_flag_bits(text):
    """Read the flag bit definitions.

    Each definition has the form "#define HS_FLAG_<name> (1u << <bit>)".

    Args:
        text (str): Contents of the C header.

    Returns:
        dict[int, str]: Bit number to flag name without the HS_FLAG_ prefix.
    """
    bits = {}
    for name, bit in re.findall(r"#define HS_FLAG_(\w+)\s+\(1u << (\d+)\)", text):
        bits[int(bit)] = name
    return bits


def load_names(path=TYPES_HEADER):
    """Read the flag, reject and status names from hs_types.h.

    Args:
        path (pathlib.Path, optional): Header to read. Defaults to TYPES_HEADER.

    Returns:
        HeaderNames: The names. All parts are empty when the file does not exist.

    Raises:
        SystemExit: The file exists, but the flags, the reject enum, the status enum or one of
            the two shifts is missing.
    """
    if not path.exists():
        return HeaderNames(flag_bits={}, reject_names={}, status_names={},
                           reject_shift=None, mode_shift=None)
    text = path.read_text()
    flag_bits = parse_flag_bits(text)
    reject_names = parse_enum(text, "hs_reject", "HS_REJECT_")
    status_names = parse_enum(text, "hs_status", "HS_STATUS_")
    reject_shift = re.search(r"#define HS_FLAG_REJECT_SHIFT\s+(\d+)", text)
    mode_shift = re.search(r"#define HS_FLAG_MODE_SHIFT\s+(\d+)", text)
    if not (flag_bits and reject_names and status_names and reject_shift and mode_shift):
        sys.exit(f"cannot parse flags, reject or status names from {path}")
    return HeaderNames(flag_bits, reject_names, status_names,
                       int(reject_shift.group(1)), int(mode_shift.group(1)))

def record_format(fields):
    """Make the struct format string for a record.

    Args:
        fields (list[Field]): Fields in wire order.

    Returns:
        str: Little-endian format string, for example "<BHIHH".
    """
    return "<" + "".join(WIRE_FORMAT[field.wire_type] for field in fields)


def record_crc(body):
    """Calculate the record CRC, the same way the firmware does.

    The CRC is CRC-16/CCITT-FALSE with the seed CRC_SEED.

    Args:
        body (bytes): Record bytes before the CRC field.

    Returns:
        int: The 16-bit CRC.
    """
    return binascii.crc_hqx(body, CRC_SEED)


def decode(fields, data):
    """Check one serialized record and read its fields.

    Args:
        fields (list[Field]): Fields in wire order. The last field must be record_crc16.
        data (bytes): One whole record, after reassembly.

    Returns:
        dict[str, int]: Field name to raw integer, in table order.

    Raises:
        ValueError: The length is not the size of the table, or the CRC does not match.
    """
    fmt = record_format(fields)
    size = struct.calcsize(fmt)
    if len(data) != size:
        raise ValueError(f"got {len(data)} B, table says {size} B")
    values = struct.unpack(fmt, data)
    raw = {}
    for field, value in zip(fields, values):
        raw[field.name] = value
    want = record_crc(data[:-2])
    if raw["record_crc16"] != want:
        raise ValueError(f"CRC mismatch: record has 0x{raw['record_crc16']:04x}, "
                         f"computed 0x{want:04x}")
    return raw


def format_flags(flags, names):
    """Show a flags value as text.

    Args:
        flags (int): Raw flags value.
        names (HeaderNames): Flag, reject and status names.

    Returns:
        str: The hex value and the names of the set bits, for example "0x00000401 [A B]".
            When names has both shifts, "reject=<name> mode=<name>" follows.
    """
    set_names = []
    for bit, name in sorted(names.flag_bits.items()):
        if flags >> bit & 1:
            set_names.append(name)
    bits = " ".join(set_names)
    if names.reject_shift is None or names.mode_shift is None:
        return f"0x{flags:08x} [{bits}]"
    reject = names.reject_names.get(flags >> names.reject_shift & REJECT_MASK, "?")
    mode = flags >> names.mode_shift & MODE_MASK
    mode_text = MODE_NAMES[mode] if mode < len(MODE_NAMES) else f"?{mode}"
    return f"0x{flags:08x} [{bits}] reject={reject} mode={mode_text}"


def is_not_available(field, value):
    """Check if a value is the "not available" marker of its field.

    Args:
        field (Field): Field of the value.
        value (int): Raw value.

    Returns:
        bool: True for NA_I16 in any i16 field, and for NA_U16 in a field of NA_U16_FIELDS.
    """
    if field.wire_type == "i16" and value == NA_I16:
        return True
    return field.name in NA_U16_FIELDS and value == NA_U16


def format_value(field, value, names):
    """Show one field value as text.

    Args:
        field (Field): Field of the value.
        value (int): Raw value.
        names (HeaderNames): Flag, reject and status names.

    Returns:
        str: The first of these that applies:
            - the text from format_flags() for the flags field
            - the status name for the status field, or the number if the name is unknown
            - "n/a" for a "not available" marker
            - the value times the scale, then the unit if the field has one
    """
    if field.name == "flags":
        return format_flags(value, names)
    if field.name == "status":
        return names.status_names.get(value, str(value))
    if is_not_available(field, value):
        return "n/a"
    text = str(value) if field.scale == 1 else str(round(value * field.scale, 6))
    return f"{text} {field.unit}" if field.unit else text


def flag_bit(names, flag_name):
    """Find the bit number of a flag.

    Args:
        names (HeaderNames): Flag, reject and status names.
        flag_name (str): Flag name without the HS_FLAG_ prefix.

    Returns:
        int | None: The bit number, or None if hs_types.h does not define the flag. If the
            header defines the name two times, the last bit is used.
    """
    bit = None
    for number, name in names.flag_bits.items():
        if name == flag_name:
            bit = number  # if a name is defined twice, the last definition wins
    return bit


def build_tags(raw, names):
    """Make the build tags for the start of a record line.

    Args:
        raw (dict[str, int]): Decoded record from decode().
        names (HeaderNames): Flag, reject and status names.

    Returns:
        str: "[SIM] " if BUILD_AFE_SIM is set, then "[ORACLE] " if BUILD_ORACLE is set. Empty
            when the record has no flags field. A flag that hs_types.h does not define counts
            as not set.
    """
    if "flags" not in raw:
        return ""
    tags = ""
    for tag, flag_name in BUILD_TAGS:
        bit = flag_bit(names, flag_name)
        if bit is not None and raw["flags"] >> bit & 1:  # a missing flag counts as not set
            tags += f"[{tag}] "
    return tags


def format_record(fields, raw, names):
    """Show one decoded record as one line.

    Args:
        fields (list[Field]): Fields in wire order.
        raw (dict[str, int]): Decoded record from decode().
        names (HeaderNames): Flag, reject and status names.

    Returns:
        str: The build tags, then "name=value" for each field in table order. Two spaces
            separate the fields.
    """
    parts = []
    for field in fields:
        parts.append(f"{field.name}={format_value(field, raw[field.name], names)}")
    return build_tags(raw, names) + "  ".join(parts)

class Reassembler:
    """Join fragments back into whole records.

    Each BLE notification carries one fragment. The class holds the fragments of one record at
    a time.

    Attributes:
        parts (dict): (session, sequence) to (count, {index: payload}) for the record in
            progress.
        dropped (int): Number of incomplete records that the class abandoned because a fragment
            of a different record arrived.
    """

    def __init__(self):
        """Start with no record in progress and no dropped records."""
        self.parts = {}  # (session, sequence) -> (count, {index: payload})
        self.dropped = 0  # incomplete records abandoned because a newer one started

    def feed(self, data):
        """Add one fragment.

        For a repeated fragment index, the first copy is kept. A fragment of a different record
        abandons the record in progress and adds 1 to dropped.

        Args:
            data (bytes): One notification: the 6-byte header, then record bytes.

        Returns:
            bytes | None: The whole record when this fragment completes it, else None.

        Raises:
            ValueError: The fragment is shorter than 7 bytes, count is 0, the index is not less
                than count, or count differs from the earlier fragments of the same record.
        """
        if len(data) < FRAGMENT_HEADER.size + 1:
            raise ValueError(f"fragment of {len(data)} B, need at least {FRAGMENT_HEADER.size + 1}")
        session, sequence, index, count = FRAGMENT_HEADER.unpack_from(data)
        if count == 0 or index >= count:
            raise ValueError(f"bad fragment index {index} of count {count}")
        key = (session, sequence)
        if key not in self.parts:
            self.dropped += len(self.parts)  # an incomplete older record is abandoned
            self.parts = {key: (count, {})}
        expected_count, received = self.parts[key]
        if count != expected_count:
            raise ValueError(f"fragment count {count} differs from {expected_count} "
                             f"for session {session} seq {sequence}")
        received.setdefault(index, bytes(data[FRAGMENT_HEADER.size:]))  # keep the first copy
        if len(received) < expected_count:
            return None
        del self.parts[key]
        return b"".join(received[i] for i in range(expected_count))


def gap(last, session, sequence):
    """Check if a sequence number skips ahead in its session.

    The sequence is u16 and wraps at 65536. A step of 1 to 0x7FFF counts as forward. A step of
    0, or of 0x8000 and more, counts as a repeated or older record.

    Args:
        last (dict[int, int]): Session ID to last sequence. The function updates it for the
            first record of a session, a repeated record and a forward step.
        session (int): Session ID of the record.
        sequence (int): Sequence of the record.

    Returns:
        str | None: "GAP: session <session> sequence <previous> -> <sequence>" when the step
            forward is 2 or more, else None.
    """
    previous = last.get(session)
    if previous is None:
        last[session] = sequence
        return None
    step = (sequence - previous) & SEQUENCE_MASK
    if step < HALF_SEQUENCE_RANGE:
        last[session] = sequence
    if 1 < step < HALF_SEQUENCE_RANGE:
        return f"GAP: session {session} sequence {previous} -> {sequence}"
    return None

async def run(name, count):
    """Connect to the device and print each record that it sends.

    The function scans for the device, pairs, subscribes to the record characteristic and
    prints one line for each record. It stops after count records or when the link drops.

    If the link fails before the subscribe, the bonds on the two sides may not match (one side
    lost its bond). The function then removes the host bond and tries again until a new
    PAIRING window opens or REPAIR_WAIT_S is over.

    Args:
        name (str): Advertised device name to scan for.
        count (int): Number of records to receive. 0 means no limit.

    Raises:
        SystemExit: No device has the name, the subscribe failed after the retry, the host
            unpair failed, or the link dropped before
            count records arrived (exit code 1).
    """
    from bleak import BleakClient, BleakScanner
    from bleak.exc import BleakError

    version, fields = load_table()
    names = load_names()
    print(f"table v{version}: {', '.join(field.name for field in fields)}")
    device = await BleakScanner.find_device_by_name(name, timeout=30)
    if device is None:
        sys.exit(f"no device named {name!r} found")
    print(f"found {device.name} {device.address}")

    done = asyncio.Event()
    reassembler = Reassembler()
    last_sequence = {}  # session -> last sequence seen
    received = gaps = bad = 0

    def on_notify(_, data: bytearray):
        """Decode one notification and print the result.

        Any error prints a BAD line. The session continues after an error.

        Args:
            _: The characteristic that sent the notification. Not used.
            data (bytearray): One fragment.
        """
        nonlocal received, gaps, bad
        try:
            record = reassembler.feed(bytes(data))
            if record is None:
                return
            raw = decode(fields, record)
            line = format_record(fields, raw, names)
        except Exception as e:  # a bad record must never kill the notification callback
            bad += 1
            why = e if isinstance(e, ValueError) else repr(e)
            print(f"BAD: {why} [{bytes(data).hex()}]", flush=True)
            return
        if raw["version"] != version:
            print(f"WARNING: record v{raw['version']} but table is v{version}")
        gap_message = gap(last_sequence, raw["session_id"], raw["sequence"])
        if gap_message:
            print(gap_message)
            gaps += 1
        received += 1
        print(f"#{received} [{record.hex()}]  {line}", flush=True)
        if count and received >= count:
            done.set()

    live = False  # True while a session is subscribed

    def on_disconnect(_):
        """Stop the session when the device drops the link.

        Args:
            _: The client. Not used.
        """
        if live and not done.is_set():  # done is already set when count ended the session
            print("link dropped by the device", flush=True)
        done.set()

    async def session(device):
        """Connect, pair if needed, subscribe and receive until done is set.

        The record CCC needs an encrypted link, so the client pairs first. Pairing is skipped
        if the host is already bonded. The first bond needs the device's PAIRING window (long
        press, blue SLOW).

        Args:
            device (BLEDevice): Device from the scan.

        Returns:
            str | None: None after a session that subscribed. Else the reason that the link
                failed before notifications were on.
        """
        nonlocal live
        done.clear()
        try:
            async with BleakClient(device, disconnected_callback=on_disconnect,
                                   pair=True) as client:
                await client.start_notify(RECORD_UUID, on_notify)
                live = True
                print(f"connected, MTU {client.mtu_size}", flush=True)
                await done.wait()
        except (BleakError, asyncio.TimeoutError, OSError) as e:
            if live:
                raise
            return str(e) or type(e).__name__
        finally:
            live = False
        return None

    why = await session(device)
    if why is not None:
        # The bonds on the two sides do not match. Two cases:
        # - The device lost its bond (for example after an erase). The host encrypts with its
        #   old key, the device has no key to match, and the host never pairs again by itself.
        # - The host lost its bond. The device refuses pairing outside the PAIRING window.
        # Both recover with no host bond and a pairing in a new PAIRING window.
        print(f"connect failed ({why}). The bonds on the host and the device do not match. "
              "Removing the host bond.", flush=True)
        try:
            await BleakClient(device).unpair()
        except (BleakError, NotImplementedError, OSError) as e:
            sys.exit(f"unpair failed ({e}). Remove the device in the host Bluetooth "
                     "settings, long-press the device, then run again.")
        print(f"Long-press the device to open pairing (blue SLOW). Trying for "
              f"{REPAIR_WAIT_S} s.", flush=True)
        # A device with a bond advertises outside the window too, so one attempt can come too
        # early. Try again until the window opens or the time is up.
        loop = asyncio.get_running_loop()
        deadline = loop.time() + REPAIR_WAIT_S
        while why is not None and loop.time() < deadline:
            await asyncio.sleep(REPAIR_RETRY_S)
            found = await BleakScanner.find_device_by_name(
                name, timeout=max(1.0, deadline - loop.time()))
            if found is not None:
                why = await session(found)
        if why is not None:
            sys.exit(f"pairing failed ({why}). If the device still refuses, run "
                     "'hs ble unpair' on the device shell, long-press the device, then run "
                     "again.")
    print(f"disconnected after {received} records, {gaps} sequence gaps, "
          f"{reassembler.dropped} dropped partials, {bad} bad")
    if count and received < count:
        sys.exit(1)

# Bytes 0-14 are copied from test_golden_offsets in firmware/tests/packet/src/main.c. The CRC
# (bytes 15-16, 0xd39c LSB first) is CRC-16/CCITT-FALSE of those bytes, computed once by a separate
# bitwise implementation, not by this script.
GOLDEN = "0208070c0b0a090e0d1211141316159cd3"


def raises_value_error(fn, *args):
    """Check if a call raises ValueError.

    Args:
        fn (callable): Function to call.
        *args: Arguments for fn.

    Returns:
        bool: True if fn(*args) raised ValueError. False if it returned. Other exceptions are
            not caught.
    """
    try:
        fn(*args)
    except ValueError:
        return True
    return False


def fragment(session, sequence, index, count, payload):
    """Make one fragment for a test.

    Args:
        session (int): Session ID for the header.
        sequence (int): Sequence for the header.
        index (int): Fragment index.
        count (int): Number of fragments in the record.
        payload (bytes): Record bytes of this fragment.

    Returns:
        bytes: The header, then payload.
    """
    return FRAGMENT_HEADER.pack(session, sequence, index, count) + payload


def sample_value(index, field):
    """Make a distinct test value for one field.

    Args:
        index (int): Position of the field in the table.
        field (Field): The field.

    Returns:
        int: 1 for version, -100 - index for i16, 10 + index for u8, and 1000 + index for the
            other types.
    """
    if field.name == "version":
        return 1
    if field.wire_type == "i16":
        return -100 - index
    if field.wire_type == "u8":
        return 10 + index
    return 1000 + index


def test_table_shape(version, fields):
    """Check that the header gives packet v2.

    Packet v2 has 8 fields in a known order, 17 bytes and only known wire types.

    Args:
        version (int): Version from load_table().
        fields (list[Field]): Fields from load_table().

    Raises:
        AssertionError: The header does not match packet v2.
    """
    size = struct.calcsize(record_format(fields))
    names = [field.name for field in fields]
    assert version == 2 and len(fields) == 8 and size == 17, (version, len(fields), size)
    assert names == ["version", "session_id", "timestamp_s", "sequence", "temp_centi_c",
                     "a_rms_mm_s2", "battery_mv", "record_crc16"], names
    assert all(field.wire_type in WIRE_FORMAT for field in fields)


def test_crc():
    """Check the CRC against the published CRC-16/CCITT-FALSE check value 0x29B1.

    Raises:
        AssertionError: The CRC is wrong.
    """
    assert record_crc(b"123456789") == 0x29B1


def test_round_trip(fields):
    """Pack distinct values into a record and check that decode() gives them back.

    Args:
        fields (list[Field]): Fields from load_table().

    Raises:
        AssertionError: A decoded value differs from the packed value.
    """
    body_fields = fields[:-1]  # everything before record_crc16
    values = []
    for index, field in enumerate(body_fields):
        values.append(sample_value(index, field))
    body = struct.pack(record_format(body_fields), *values)
    record = body + struct.pack("<H", record_crc(body))
    raw = decode(fields, record)
    assert list(raw.values()) == values + [record_crc(body)], raw


def test_golden(fields, names):
    """Decode the record from the firmware unit test and check its values and its line.

    Args:
        fields (list[Field]): Fields from load_table().
        names (HeaderNames): Names from load_names().

    Raises:
        AssertionError: A value or the printed line is wrong.
    """
    raw = decode(fields, bytes.fromhex(GOLDEN))
    want = dict(version=2, session_id=0x0708, timestamp_s=0x090A0B0C, sequence=0x0D0E,
                temp_centi_c=0x1112, a_rms_mm_s2=0x1314, battery_mv=0x1516, record_crc16=0xD39C)
    assert raw == want, raw
    line = format_record(fields, raw, names)
    assert line == ("version=2  session_id=1800  timestamp_s=151653132 s  sequence=3342  "
                    "temp_centi_c=43.7 C  a_rms_mm_s2=4.884 m/s^2  battery_mv=5398 mV  "
                    "record_crc16=54172"), line


def test_bad_records(fields):
    """Check that decode() rejects damaged records.

    The test flips one bit, the same as test_parse_rejects in the firmware. It also removes one
    byte and adds one byte.

    Args:
        fields (list[Field]): Fields from load_table().

    Raises:
        AssertionError: decode() accepted a damaged record.
    """
    golden = bytes.fromhex(GOLDEN)
    flipped = bytearray(golden)
    flipped[4] ^= 0x04
    assert raises_value_error(decode, fields, bytes(flipped))
    assert raises_value_error(decode, fields, golden[:-1])
    assert raises_value_error(decode, fields, golden + b"\x00")


def test_row_regex():
    """Check that FIELD_ROW accepts each scale form and needs the unit string.

    The scale forms are an integer, a fraction, a negative number, an exponent and a leading
    decimal point.

    Raises:
        AssertionError: FIELD_ROW rejected a valid row or accepted a row with no unit.
    """
    for scale in ("1", "0.01", "-0.5", "1e-3", "2.5E+2", ".1"):
        row = FIELD_ROW.match(f'    X(f, i16, {scale}, "ohm") \\')
        assert row and float(row.group(3)) == float(scale), scale
    assert not FIELD_ROW.match("    X(f, u8, 1) \\")


def test_flags(fields, names):
    """Check the flag handling.

    Packet v2 has no flags or status field. A flags field shows no build tags when the names do
    not define BUILD_AFE_SIM or BUILD_ORACLE. An unknown mode shows as "?<value>".

    Args:
        fields (list[Field]): Fields from load_table().
        names (HeaderNames): Names from load_names().

    Raises:
        AssertionError: The flags text is wrong, or a missing flag shows a tag.
    """
    assert not any(field.name in ("flags", "status") for field in fields)
    if not TYPES_HEADER.exists():
        assert names == HeaderNames(flag_bits={}, reject_names={}, status_names={},
                                    reject_shift=None, mode_shift=None)
    flags_table = [Field("version", "u8", 1.0, ""), Field("flags", "u32", 1.0, ""),
                   Field("record_crc16", "u16", 1.0, "")]
    some_names = HeaderNames(flag_bits={0: "A"}, reject_names={}, status_names={},
                             reject_shift=16, mode_shift=20)
    for header_names in (names, some_names):
        line = format_record(flags_table, {"version": 1, "flags": 0xFFFFFFFF, "record_crc16": 0},
                             header_names)
        assert "[SIM]" not in line and "[ORACLE]" not in line, line
        assert "flags=0xffffffff" in line, line
    assert "mode=?3" in format_flags(3 << 20, some_names)
    assert format_flags(1, names) == "0x00000001 []"


def test_reassembly():
    """Check that Reassembler joins fragments and rejects bad ones.

    The test covers these cases:
        - 2 fragments of 20 and 9 bytes at the default 23-byte MTU
        - 3 fragments in order, out of order, and with a repeated fragment
        - a newer record that abandons an incomplete record
        - a fragment that is too short, an index past count, and count 0

    Raises:
        AssertionError: A record was joined incorrectly, or a bad fragment was accepted.
    """
    golden = bytes.fromhex(GOLDEN)
    # Default 23-byte MTU (20-byte notifications): 2 fragments carrying 14 and 3 record bytes.
    assert len(fragment(0x0708, 0x0D0E, 0, 2, golden[:14])) == 20
    assert len(fragment(0x0708, 0x0D0E, 1, 2, golden[14:])) == 9
    # 12-byte notifications: 3 fragments carrying 6, 6 and 5 record bytes.
    frags = []
    for index in range(3):
        frags.append(fragment(0x0708, 0x0D0E, index, 3, golden[index * 6:(index + 1) * 6]))
    assert [len(f) for f in frags] == [12, 12, 11]
    for order in ((0, 1, 2), (2, 0, 1), (1, 2, 1, 0)):  # the last order repeats fragment 1
        reassembler, record = Reassembler(), None
        for index in order:
            record = reassembler.feed(frags[index]) or record
        assert record == golden and reassembler.dropped == 0, order
    assert Reassembler().feed(fragment(1, 2, 0, 1, golden)) == golden

    # A fragment of a newer record abandons the incomplete older one.
    reassembler = Reassembler()
    assert reassembler.feed(frags[0]) is None
    assert reassembler.feed(fragment(7, 259, 0, 2, b"xx")) is None and reassembler.dropped == 1
    assert reassembler.feed(fragment(7, 259, 1, 2, b"yy")) == b"xxyy" and reassembler.dropped == 1

    # Too short, index past count, and count 0.
    for bad in (frags[0][:6], fragment(1, 2, 3, 3, b"x"), fragment(1, 2, 0, 0, b"x")):
        assert raises_value_error(Reassembler().feed, bad)


def test_gaps():
    """Check that only a forward skip in the same session is a gap.

    The test also checks the smallest gap and a gap across the u16 wrap. It prints the GAP
    messages that it finds, so the --selftest output stays the same as before.

    Raises:
        AssertionError: gap() found a gap that is not there, or missed one.
    """
    last = {}
    messages = [gap(last, 1, 10), gap(last, 1, 11), gap(last, 1, 14), gap(last, 1, 12),
                gap(last, 2, 0)]
    found = [message is not None for message in messages]
    assert found == [False, False, True, False, False], messages
    # The smallest gap: one record missing (10 -> 12), and across the u16 wrap (65535 -> 1).
    assert gap({1: 10}, 1, 12) == "GAP: session 1 sequence 10 -> 12"
    assert gap({1: 65535}, 1, 1) == "GAP: session 1 sequence 65535 -> 1"
    assert gap({1: 65535}, 1, 0) is None
    for message in messages:  # printed so --selftest output stays the same as before
        if message:
            print(message)


def selftest():
    """Run all checks and print a one-line summary.

    Raises:
        AssertionError: A check failed.
        SystemExit: A header cannot be parsed.
    """
    version, fields = load_table()
    names = load_names()
    test_table_shape(version, fields)
    test_crc()
    test_round_trip(fields)
    test_golden(fields, names)
    test_bad_records(fields)
    test_row_regex()
    test_flags(fields, names)
    test_reassembly()
    test_gaps()
    size = struct.calcsize(record_format(fields))
    print(f"selftest ok: v{version}, {size} B, {len(fields)} fields, crc, golden, reassembly, gaps")

def main():
    """Read the command line and run the selected mode.

    The modes are checked in this order: --selftest, --hex, then live mode.

    Raises:
        SystemExit: The --hex input is not a valid record (exit code 1), an argument is not
            valid (exit code 2), or the selected mode stopped with an error.
    """
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--name", default=DEVICE_NAME)
    parser.add_argument("--count", type=int, default=0, help="stop after N records (0 = forever)")
    parser.add_argument("--hex", help="decode one serialized record given as hex")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    if args.selftest:
        selftest()
    elif args.hex is not None:
        _, fields = load_table()
        try:
            data = bytes.fromhex("".join(args.hex.split()))
            print(format_record(fields, decode(fields, data), load_names()))
        except ValueError as e:
            sys.exit(f"error: {e}")
    else:
        asyncio.run(run(args.name, args.count))


if __name__ == "__main__":
    main()
