#include "app.h"

#include <math.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hw290_imu.h"
#include "app_config.h"

static void filter_sample(const imu_sample_t *input, imu_sample_t *state)
{
    for (int axis = 0; axis < 3; ++axis) {
        state->accel_g[axis] += APP_FILTER_ALPHA *
                                (input->accel_g[axis] - state->accel_g[axis]);
        state->gyro_dps[axis] += APP_FILTER_ALPHA *
                                 (input->gyro_dps[axis] - state->gyro_dps[axis]);
    }
}

static float vector_magnitude(const float values[3])
{
    return sqrtf(values[0] * values[0] + values[1] * values[1] + values[2] * values[2]);
}

void app_main_start(void)
{
    while (imu_init() != ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    imu_sample_t calibration_sum = {0};
    imu_sample_t sample;
    int valid_samples = 0;
    while (valid_samples < APP_CALIBRATION_SAMPLES) {
        if (imu_read_sample(&sample) == ESP_OK) {
            for (int axis = 0; axis < 3; ++axis) {
                calibration_sum.accel_g[axis] += sample.accel_g[axis];
                calibration_sum.gyro_dps[axis] += sample.gyro_dps[axis];
            }
            ++valid_samples;
        }
        vTaskDelay(pdMS_TO_TICKS(APP_CALIBRATION_PERIOD_MS));
    }

    imu_sample_t gyro_bias = {0};
    imu_sample_t filter_state = {0};
    for (int axis = 0; axis < 3; ++axis) {
        filter_state.accel_g[axis] = calibration_sum.accel_g[axis] / valid_samples;
        gyro_bias.gyro_dps[axis] = calibration_sum.gyro_dps[axis] / valid_samples;
        filter_state.gyro_dps[axis] = 0.0f;
    }
    const float stationary_accel_magnitude = vector_magnitude(filter_state.accel_g);

    bool motion_detected = false;
    int motion_samples = 0;
    int quiet_samples = 0;
    float accel_magnitude = stationary_accel_magnitude;
    float gyro_magnitude = 0.0f;
    TickType_t last_eval = xTaskGetTickCount();
    TickType_t last_output = last_eval;

    while (true) {
        if (imu_read_sample(&sample) == ESP_OK) {
            for (int axis = 0; axis < 3; ++axis) {
                sample.gyro_dps[axis] -= gyro_bias.gyro_dps[axis];
            }
            filter_sample(&sample, &filter_state);

            accel_magnitude = vector_magnitude(filter_state.accel_g);
            gyro_magnitude = vector_magnitude(filter_state.gyro_dps);
            float accel_deviation = fabsf(accel_magnitude - stationary_accel_magnitude);
            bool motion_candidate =
                accel_deviation >= APP_MOTION_ACCEL_ENTER_G ||
                gyro_magnitude >= APP_MOTION_GYRO_ENTER_DPS;
            bool quiet_candidate =
                accel_deviation <= APP_MOTION_ACCEL_CLEAR_G &&
                gyro_magnitude <= APP_MOTION_GYRO_CLEAR_DPS;

            if (!motion_detected) {
                quiet_samples = 0;
                if (motion_candidate) {
                    ++motion_samples;
                    if (motion_samples >= APP_MOTION_ENTER_SAMPLES) {
                        motion_detected = true;
                        motion_samples = 0;
                    }
                } else {
                    motion_samples = 0;
                }
            } else {
                motion_samples = 0;
                if (quiet_candidate) {
                    ++quiet_samples;
                    if (quiet_samples >= APP_MOTION_CLEAR_SAMPLES) {
                        motion_detected = false;
                        quiet_samples = 0;
                    }
                } else {
                    quiet_samples = 0;
                }
            }
        }

        TickType_t now = xTaskGetTickCount();
        if ((now - last_output) >= pdMS_TO_TICKS(APP_SAMPLE_PERIOD_MS)) {
            printf("ACC [g] X:%+.3f Y:%+.3f Z:%+.3f MAG:%+.3f | GYRO [dps] X:%+.3f Y:%+.3f Z:%+.3f MAG:%+.3f | STATUS:%s\n",
                   filter_state.accel_g[0], filter_state.accel_g[1], filter_state.accel_g[2],
                   accel_magnitude,
                   filter_state.gyro_dps[0], filter_state.gyro_dps[1], filter_state.gyro_dps[2],
                   gyro_magnitude,
                   motion_detected ? "MOTION DETECTED" : "NORMAL");
            fflush(stdout);
            last_output = now;
        }
        vTaskDelayUntil(&last_eval, pdMS_TO_TICKS(APP_MOTION_EVAL_PERIOD_MS));
    }
}
