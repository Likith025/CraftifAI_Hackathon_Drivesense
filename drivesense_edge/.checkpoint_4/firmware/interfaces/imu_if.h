#ifndef IMU_IF_H
#define IMU_IF_H

#include "esp_err.h"

typedef struct {
    float accel_g[3];
    float gyro_dps[3];
} imu_sample_t;

esp_err_t imu_init(void);
esp_err_t imu_read_sample(imu_sample_t *sample);

#endif
