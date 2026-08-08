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

    TickType_t last_wake = xTaskGetTickCount();
    while (true) {
        if (imu_read_sample(&sample) == ESP_OK) {
            for (int axis = 0; axis < 3; ++axis) {
                sample.gyro_dps[axis] -= gyro_bias.gyro_dps[axis];
            }
            filter_sample(&sample, &filter_state);

            float accel_magnitude = sqrtf(
                filter_state.accel_g[0] * filter_state.accel_g[0] +
                filter_state.accel_g[1] * filter_state.accel_g[1] +
                filter_state.accel_g[2] * filter_state.accel_g[2]);
            float gyro_magnitude = sqrtf(
                filter_state.gyro_dps[0] * filter_state.gyro_dps[0] +
                filter_state.gyro_dps[1] * filter_state.gyro_dps[1] +
                filter_state.gyro_dps[2] * filter_state.gyro_dps[2]);

            printf("ACC [g] X:%+.3f Y:%+.3f Z:%+.3f MAG:%+.3f | GYRO [dps] X:%+.3f Y:%+.3f Z:%+.3f MAG:%+.3f\n",
                   filter_state.accel_g[0], filter_state.accel_g[1], filter_state.accel_g[2],
                   accel_magnitude,
                   filter_state.gyro_dps[0], filter_state.gyro_dps[1], filter_state.gyro_dps[2],
                   gyro_magnitude);
            fflush(stdout);
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(APP_SAMPLE_PERIOD_MS));
    }
}
