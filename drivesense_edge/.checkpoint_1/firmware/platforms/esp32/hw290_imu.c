#include "hw290_imu.h"

#include <stdint.h>
#include "app_config.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_dev;

#define REG_SMPLRT_DIV  0x19
#define REG_CONFIG      0x1A
#define REG_GYRO_CONFIG 0x1B
#define REG_ACCEL_CONFIG 0x1C
#define REG_ACCEL_XOUT_H 0x3B
#define REG_PWR_MGMT_1  0x6B
#define REG_WHO_AM_I    0x75

static esp_err_t reg_read(uint8_t reg, uint8_t *data, size_t length)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, data, length,
                                       pdMS_TO_TICKS(100));
}

static esp_err_t reg_write(uint8_t reg, uint8_t value)
{
    uint8_t buffer[2] = {reg, value};
    return i2c_master_transmit(s_dev, buffer, sizeof(buffer),
                                pdMS_TO_TICKS(100));
}

esp_err_t imu_init(void)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = APP_I2C_SDA_GPIO,
        .scl_io_num = APP_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&bus_config, &s_bus);
    if (err != ESP_OK) {
        return err;
    }

    i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = APP_IMU_I2C_ADDRESS,
        .scl_speed_hz = APP_I2C_FREQUENCY_HZ,
    };
    err = i2c_master_bus_add_device(s_bus, &device_config, &s_dev);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t who_am_i = 0;
    err = reg_read(REG_WHO_AM_I, &who_am_i, 1);
    if (err != ESP_OK) {
        return err;
    }
    if (who_am_i != 0x68 && who_am_i != 0x70 && who_am_i != 0x71 && who_am_i != 0x73) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    ESP_ERROR_CHECK(reg_write(REG_PWR_MGMT_1, 0x00));
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_ERROR_CHECK(reg_write(REG_CONFIG, 0x03));
    ESP_ERROR_CHECK(reg_write(REG_SMPLRT_DIV, 0x07));
    ESP_ERROR_CHECK(reg_write(REG_GYRO_CONFIG, 0x00));
    ESP_ERROR_CHECK(reg_write(REG_ACCEL_CONFIG, 0x00));

    return ESP_OK;
}

esp_err_t imu_read_sample(imu_sample_t *sample)
{
    if (sample == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t raw[14];
    esp_err_t err = reg_read(REG_ACCEL_XOUT_H, raw, sizeof(raw));
    if (err != ESP_OK) {
        return err;
    }

    int16_t ax = (int16_t)((raw[0] << 8) | raw[1]);
    int16_t ay = (int16_t)((raw[2] << 8) | raw[3]);
    int16_t az = (int16_t)((raw[4] << 8) | raw[5]);
    int16_t gx = (int16_t)((raw[8] << 8) | raw[9]);
    int16_t gy = (int16_t)((raw[10] << 8) | raw[11]);
    int16_t gz = (int16_t)((raw[12] << 8) | raw[13]);

    sample->accel_g[0] = (float)ax / 16384.0f;
    sample->accel_g[1] = (float)ay / 16384.0f;
    sample->accel_g[2] = (float)az / 16384.0f;
    sample->gyro_dps[0] = (float)gx / 131.0f;
    sample->gyro_dps[1] = (float)gy / 131.0f;
    sample->gyro_dps[2] = (float)gz / 131.0f;
    return ESP_OK;
}
