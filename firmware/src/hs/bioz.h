#ifndef BIOZ_H
#define BIOZ_H

/**
 * @brief Converts the raw AFE readings to impedance.
 *
 * @retval 0         Success.
 * @retval -ENOTSUP  The function is not implemented.
 */
int bioz_calibrate(void);

/**
 * @brief Fits the Cole model to the impedance and calculates the features.
 *
 * @retval 0         Success.
 * @retval -ENOTSUP  The function is not implemented.
 */
int bioz_fit(void);

/**
 * @brief Checks the quality of the measurement.
 *
 * @retval 0         Success.
 * @retval -ENOTSUP  The function is not implemented.
 */
int bioz_quality(void);

/**
 * @brief Compares the features to the baseline and calculates the score.
 *
 * @retval 0         Success.
 * @retval -ENOTSUP  The function is not implemented.
 */
int bioz_score(void);

#endif  // BIOZ_H
