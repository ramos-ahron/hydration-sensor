#ifndef AFE_H
#define AFE_H

/**
 * @brief Starts the AFE.
 *
 * A reset of the MCU does not reset the MAX30009. INIT calls afe_stop() after this function to
 * stop a drive that is still on.
 *
 * @retval 0         Success.
 * @retval -ENOTSUP  The function is not implemented.
 */
int afe_init(void);

/**
 * @brief Runs one impedance sweep and reads the raw samples.
 *
 * The drive is on while this function runs. The function stops the drive before it returns. Call
 * safety_drive_feed() at least every DRIVE_WDT_MS while the drive is on.
 *
 * @retval 0         Success.
 * @retval -ENOTSUP  The function is not implemented.
 */
int afe_measure(void);

/**
 * @brief Stops the drive current.
 *
 * It is safe to call this function at any time, and more than one time.
 *
 * @retval 0         Success.
 * @retval -ENOTSUP  The function is not implemented.
 */
int afe_stop(void);

#endif  // AFE_H
