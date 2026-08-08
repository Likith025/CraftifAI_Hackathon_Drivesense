# HW290 10DOF IMU Reader

ESP-IDF firmware for the ESP32-C3-DevKitM-1. It reads acceleration and gyroscope values from the MPU-compatible IMU on an HW290 10DOF sensor over I2C and prints all three axes every 500 ms.

- SDA: GPIO8
- SCL: GPIO10
- I2C address: `0x68`
- Bus speed: 400 kHz
- Acceleration: g, ±2 g configuration
- Gyroscope: degrees/second, ±250 dps configuration

Build with the ESP-IDF project tools. The onboard RGB LED shares GPIO8 with SDA and may interfere with I2C; use external pull-ups and move SDA to GPIO6 or GPIO7 if needed.
