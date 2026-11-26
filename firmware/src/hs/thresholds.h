#ifndef THRESHOLDS_H
#define THRESHOLDS_H

/* Pipeline timeouts */
#define DRIVE_WDT_MS 8000

/* Device timing */
#define RESULT_DISPLAY_MS 10000
#define SLEEP_GUARD_MS    10000
#define BATTERY_PERIOD_MS 30000

/* Battery checks */
#define BATT_LOW_MV      2700
#define BATT_CRITICAL_MV 2400
#define BATT_MARGIN_MV   100

/* Motion */
#define ACCEL_WINDOW_SAMPLES   32
#define ACCEL_SAMPLE_PERIOD_MS 10

#endif  // THRESHOLDS_H
