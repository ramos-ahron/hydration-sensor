#ifndef PIPELINE_H
#define PIPELINE_H

#include "hs/types.h"

/**
 * @brief Starts the measurement step on the pipeline work queue.
 *
 * The function returns at once. The step clears the result, checks safety and runs afe_measure().
 * Then it posts one event:
 * - ACQUIRED_SAMPLES: the sweep is done, or safety_check() failed. For a safety failure, the
 *   result holds REJECT_CHARGING and pipeline_process() does not run the math.
 * - SENSOR_FAULT: afe_measure() failed. The result holds REJECT_AFE.
 *
 * A stub that returns -ENOTSUP is "not implemented". The step logs a warning and continues.
 */
void pipeline_measure(void);

/**
 * @brief Starts the processing step on the pipeline work queue.
 *
 * The function returns at once. The step runs the BioZ math in order and stops at the first
 * failure. A failure sets REJECT_MATH in the result. -ENOTSUP is not a failure. The step always
 * posts COMPUTE_DONE.
 */
void pipeline_process(void);

/**
 * @brief Gets the result of the last measurement.
 *
 * @return The result. It is valid after COMPUTE_DONE, until the next pipeline_measure().
 */
const struct result *pipeline_result(void);

/**
 * @brief Cancels a pending step, waits for a running step to end and stops the drive.
 *
 * The drive is off when the function returns, and the FSM thread may use the AFE. A step that
 * ended may still have posted its event. The function blocks, so call it only from the FSM
 * thread.
 */
void pipeline_stop(void);

#endif  // PIPELINE_H
