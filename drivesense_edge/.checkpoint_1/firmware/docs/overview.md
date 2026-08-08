# HW290 10DOF IMU Reader

## Purpose

This ESP-IDF firmware reads the acceleration and gyroscope axes from the MPU-compatible IMU on an HW290 10DOF sensor and prints only the converted acceleration and gyroscope values to the serial monitor every 2 seconds.

## Hardware map

- ESP32-C3-DevKitM-1
- I²C SDA: GPIO8
- I²C SCL: GPIO10
- Sensor address: `0x68`
- I²C speed: 400 kHz
- Acceleration output: X/Y/Z in g, configured for ±2 g
- Gyroscope output: X/Y/Z in degrees per second, configured for ±250 dps

GPIO8 is also connected to the board RGB LED. Use suitable external I²C pull-ups and move SDA to GPIO6 or GPIO7 if the LED connection affects the bus.

## Firmware modules

- `firmware/app/app.c`: startup, retry handling, periodic sampling, and serial logging.
- `firmware/platforms/esp32/hw290_imu.c`: ESP-IDF I²C bus setup, sensor initialization, register access, and conversion.
- `firmware/interfaces/imu_if.h`: platform-neutral IMU sample contract.
- `firmware/configs/app_config.h`: application pins, address, speed, and sample period.

## Build and run

Build the project for `esp32c3`, flash the generated firmware, and open the serial monitor at 115200 baud. Successful startup reports the sensor `WHO_AM_I` value, followed by acceleration and gyroscope values at approximately 2-second intervals.
