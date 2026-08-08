#include "app.h"

#include <math.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hw290_imu.h"
#include "app_config.h"

typedef enum {
    EVENT_NORMAL,
    EVENT_HARSH_ACCEL_BRAKING,
    EVENT_AGGRESSIVE_CORNERING,
    EVENT_ROAD_IMPACT,
    EVENT_MOTION_DETECTED
} event_status_t;

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

static const char *event_status_name(event_status_t status)
{
    switch (status) {
    case EVENT_ROAD_IMPACT:
        return "ROAD IMPACT";
    case EVENT_AGGRESSIVE_CORNERING:
        return "AGGRESSIVE CORNERING";
    case EVENT_HARSH_ACCEL_BRAKING:
        return "HARSH ACCEL/BRAKING";
    case EVENT_MOTION_DETECTED:
        return "MOTION DETECTED";
    default:
        return "NORMAL";
    }
}

static bool pattern_confirmed(const bool history[APP_EVENT_HISTORY_SAMPLES], int required)
{
    int count = 0;
    for (int i = 0; i < APP_EVENT_HISTORY_SAMPLES; ++i) {
        count += history[i] ? 1 : 0;
    }
    return count >= required;
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
    const float baseline_x = filter_state.accel_g[0];
    const float baseline_y = filter_state.accel_g[1];

    bool accel_history[APP_EVENT_HISTORY_SAMPLES] = {false};
    bool corner_history[APP_EVENT_HISTORY_SAMPLES] = {false};
    bool impact_history[APP_EVENT_HISTORY_SAMPLES] = {false};
    bool generic_history[APP_EVENT_HISTORY_SAMPLES] = {false};
    int history_index = 0;
    int quiet_samples = 0;
    event_status_t status = EVENT_NORMAL;
    TickType_t last_eval = xTaskGetTickCount();
    TickType_t last_output = last_eval;
    TickType_t impact_start = 0;
    bool impact_pending = false;
    float previous_impact_gyro = 0.0f;
    float accel_magnitude = stationary_accel_magnitude;
    float gyro_magnitude = 0.0f;

    while (true) {
        if (imu_read_sample(&sample) == ESP_OK) {
            for (int axis = 0; axis < 3; ++axis) {
                sample.gyro_dps[axis] -= gyro_bias.gyro_dps[axis];
            }
            filter_sample(&sample, &filter_state);

            accel_magnitude = vector_magnitude(filter_state.accel_g);
            gyro_magnitude = vector_magnitude(filter_state.gyro_dps);
            float x_change = fabsf(filter_state.accel_g[0] - baseline_x);
            float y_change = fabsf(filter_state.accel_g[1] - baseline_y);
            float impact_gyro = fabsf(filter_state.gyro_dps[APP_EVENT_IMPACT_GYRO_AXIS]);
            float impact_gyro_step = fabsf(impact_gyro - previous_impact_gyro);
            bool harsh_candidate = x_change >= APP_EVENT_ACCEL_THRESHOLD_G;
            bool generic_candidate =
                fabsf(accel_magnitude - stationary_accel_magnitude) >=
                    APP_EVENT_GENERIC_ACCEL_THRESHOLD_G ||
                gyro_magnitude >= APP_EVENT_GENERIC_GYRO_THRESHOLD_DPS;
            bool corner_candidate =
                y_change >= APP_EVENT_CORNER_ACCEL_THRESHOLD_G &&
                fabsf(filter_state.gyro_dps[APP_EVENT_CORNER_GYRO_AXIS]) >=
                    APP_EVENT_CORNER_GYRO_THRESHOLD_DPS;

            if (!impact_pending && impact_gyro >= APP_EVENT_IMPACT_GYRO_THRESHOLD_DPS &&
                impact_gyro_step >= APP_EVENT_IMPACT_GYRO_DELTA_DPS) {
                impact_pending = true;
                impact_start = xTaskGetTickCount();
            }
            bool impact_recovered = impact_pending &&
                impact_gyro <= APP_EVENT_IMPACT_GYRO_RECOVERY_DPS;
            if (impact_recovered &&
                (xTaskGetTickCount() - impact_start) <= pdMS_TO_TICKS(APP_EVENT_IMPACT_WINDOW_MS)) {
                impact_history[history_index] = true;
                impact_pending = false;
            } else if (impact_pending &&
                       (xTaskGetTickCount() - impact_start) > pdMS_TO_TICKS(APP_EVENT_IMPACT_WINDOW_MS)) {
                impact_pending = false;
                impact_history[history_index] = false;
            } else {
                impact_history[history_index] = false;
            }

            accel_history[history_index] = harsh_candidate;
            corner_history[history_index] = corner_candidate;
            generic_history[history_index] = generic_candidate;
            previous_impact_gyro = impact_gyro;
            history_index = (history_index + 1) % APP_EVENT_HISTORY_SAMPLES;

            bool impact_detected = pattern_confirmed(impact_history, APP_EVENT_IMPACT_CONFIRM_SAMPLES);
            bool corner_detected = pattern_confirmed(corner_history, APP_EVENT_CORNER_CONFIRM_SAMPLES);
            bool accel_detected = pattern_confirmed(accel_history, APP_EVENT_ACCEL_CONFIRM_SAMPLES);
            bool generic_detected = pattern_confirmed(generic_history, APP_EVENT_GENERIC_CONFIRM_SAMPLES);
            event_status_t detected = impact_detected ? EVENT_ROAD_IMPACT :
                                     corner_detected ? EVENT_AGGRESSIVE_CORNERING :
                                     accel_detected ? EVENT_HARSH_ACCEL_BRAKING :
                                     generic_detected ? EVENT_MOTION_DETECTED : EVENT_NORMAL;

            if (detected != EVENT_NORMAL) {
                status = detected;
                quiet_samples = 0;
            } else if (status != EVENT_NORMAL) {
                ++quiet_samples;
                if (quiet_samples >= APP_EVENT_CLEAR_SAMPLES) {
                    status = EVENT_NORMAL;
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
                   event_status_name(status));
            fflush(stdout);
            last_output = now;
        }
        vTaskDelayUntil(&last_eval, pdMS_TO_TICKS(APP_MOTION_EVAL_PERIOD_MS));
    }
}
