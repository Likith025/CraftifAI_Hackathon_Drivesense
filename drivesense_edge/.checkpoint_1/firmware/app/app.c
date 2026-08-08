#include "app.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hw290_imu.h"
#include "app_config.h"

void app_main_start(void)
{
    while (imu_init() != ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    TickType_t last_wake = xTaskGetTickCount();
    while (true) {
        imu_sample_t sample;
        if (imu_read_sample(&sample) == ESP_OK) {
            printf("ACC [g] X:%+.3f Y:%+.3f Z:%+.3f | GYRO [dps] X:%+.3f Y:%+.3f Z:%+.3f\n",
                   sample.accel_g[0], sample.accel_g[1], sample.accel_g[2],
                   sample.gyro_dps[0], sample.gyro_dps[1], sample.gyro_dps[2]);
            fflush(stdout);
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(APP_SAMPLE_PERIOD_MS));
    }
}
