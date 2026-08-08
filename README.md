# DriveSense Edge

ESP-IDF firmware for an ESP32-C3-DevKitM-1 connected to an HW290 10DOF sensor using its MPU6050-compatible accelerometer and gyroscope. The ESP32 performs calibration, filtering, motion detection, and event classification locally. It prints sensor data to the serial monitor and publishes the current event status to a laptop over TCP on the same Wi-Fi network.

## Problem definition

Vehicle motion can contain several different patterns that are useful for an edge safety or telematics system:

- Forward/backward harsh acceleration or braking
- Aggressive cornering
- Short road impacts such as potholes
- General significant movement that does not confidently match a specific event

DriveSense Edge keeps these decisions on the microcontroller for low latency and operation without a dashboard or cloud service. A laptop is currently used only as a status receiver for testing; it does not perform sensor processing or classification.

## Target users

- Embedded-systems and automotive engineering students
- Developers prototyping vehicle telemetry and event detection
- Test engineers tuning IMU thresholds using real hardware
- Researchers building a local, low-cost motion-classification platform

## Current capabilities

- MPU6050-compatible HW290 IMU over I²C
- Silent startup calibration with gyro-bias correction
- Configurable first-order low-pass filtering
- Acceleration and angular-velocity magnitudes
- Internal motion evaluation every 100 ms
- Serial sensor output every 1 second
- Event states:
  - `NORMAL`
  - `MOTION DETECTED`
  - `HARSH ACCEL/BRAKING`
  - `AGGRESSIVE CORNERING`
  - `ROAD IMPACT`
- Event priority:

```text
ROAD IMPACT > AGGRESSIVE CORNERING > HARSH ACCEL/BRAKING > MOTION DETECTED > NORMAL
```

- Wi-Fi station mode with DHCP
- Single-client TCP status server on port `5000`
- Newline-delimited status messages sent on connection, status changes, and one-second heartbeats

`CRASH` is not currently implemented.

## Hardware and wiring

### ESP32-C3 to HW290 I²C

| HW290 signal | ESP32-C3-DevKitM-1 |
|---|---:|
| VCC | 3.3 V |
| GND | GND |
| SDA | GPIO8 |
| SCL | GPIO10 |
|

Current sensor configuration:

- 7-bit I²C address: `0x68`
- I²C frequency: 400 kHz
- Accelerometer range: ±2 g
- Gyroscope range: ±250 dps

GPIO8 is also connected to the DevKit onboard RGB LED. The LED connection can interfere with I²C signal integrity. If the bus is unreliable, use external 2.2–4.7 kΩ pull-ups and move SDA to GPIO6 or GPIO7, then change only `APP_I2C_SDA_GPIO` in `app_config.h`.

### BOM

- ESP32-C3-DevKitM-1
- HW290 10DOF sensor module with MPU6050-compatible IMU
- Jumper wires or suitable connector cables
- USB data cable for power, flashing, and serial monitoring
- 2.2–4.7 kΩ I²C pull-up resistors if the module/board does not provide reliable pull-ups
- Laptop on the same Wi-Fi network for TCP status testing
- Optional smartphone mobile hotspot configured for 2.4 GHz operation

Do not connect 5 V logic directly to ESP32 GPIO pins. Confirm the sensor module's supply and logic-level requirements before wiring.

## Source code organization

```text
.
├── CMakeLists.txt                  # ESP-IDF project definition
├── main/
│   ├── CMakeLists.txt              # Component sources and dependencies
│   └── entry.c                     # Single app_main() entry point
├── firmware/
│   ├── app/
│   │   ├── app.c                   # Calibration, filtering, event state machine
│   │   └── app.h
│   ├── configs/
│   │   └── app_config.h            # Pins, thresholds, timing, Wi-Fi settings
│   ├── interfaces/
│   │   ├── imu_if.h                # IMU sample contract
│   │   └── status_transport.h      # Status publishing contract
│   ├── platforms/esp32/
│   │   ├── hw290_imu.c             # ESP-IDF I²C and MPU6050 register driver
│   │   ├── hw290_imu.h
│   │   └── wifi_tcp_status.c       # Wi-Fi station and TCP server
│   └── docs/overview.md             # Documentation landing page
└── components/firmgen_telemetry/   # IDE telemetry component; not application logic
```

`app.c` owns all sensor processing and event decisions. The laptop receives already-classified status text and does not reproduce the detection logic.

## Processing behavior

### Startup calibration

The sensor must remain stationary during startup. The firmware collects 100 valid samples at 20 ms intervals, averages the gyroscope axes, and subtracts those averages from later gyro readings. The initial accelerometer vector establishes the stationary orientation reference; it is not treated as a gravity-free accelerometer offset.

### Filtering and sampling

- Low-pass coefficient: `APP_FILTER_ALPHA = 0.30`
- Internal processing/evaluation: every 100 ms
- Serial output: every 1 second
- Output is flushed immediately after each line

### Event classification

- **HARSH ACCEL/BRAKING:** sustained calibrated X-axis acceleration deviation. Gyroscope activity alone cannot create this event.
- **AGGRESSIVE CORNERING:** sustained calibrated Y-axis acceleration together with rotation on the configured Z gyro axis.
- **ROAD IMPACT:** sudden Z gyro disturbance followed by recovery below the configured recovery level within the configured short window.
- **MOTION DETECTED:** multi-sample fallback for significant unmatched acceleration or total gyro movement.
- **NORMAL:** no event pattern is active.

The thresholds and confirmation values are in `firmware/configs/app_config.h` and are intended for physical tuning.

## Wi-Fi and laptop status transport

The ESP32 operates as a Wi-Fi **station**. It does not create a hotspot. It must join an existing 2.4 GHz network. For a phone hotspot, enable the hotspot first and set its name/password to match the configuration.

Current development credentials:

```c
#define APP_WIFI_SSID     "drivesense"
#define APP_WIFI_PASSWORD "25092002"
#define APP_TCP_STATUS_PORT 5000
```

These values are source-visible placeholders for development. Replace the password before sharing or production use.

The ESP32 obtains an IP address through DHCP and runs a single-client TCP server. It sends only one status string per line, for example:

```text
NORMAL
MOTION DETECTED
AGGRESSIVE CORNERING
ROAD IMPACT
```

### Find the ESP32 IP address

Check the phone hotspot's connected-device list or the router's DHCP lease list. The board used during validation had MAC address `A0:F2:62:01:CB:58`; the assigned IP can change after reconnecting the hotspot.

### Test the TCP port from Windows PowerShell

```powershell
Test-NetConnection <ESP32_IP> -Port 5000
```

The result should contain:

```text
TcpTestSucceeded : True
```

### Receive status with Ncat

```powershell
ncat <ESP32_IP> 5000
```

### Receive status with Python

```python
import socket

ESP32_IP = "192.168.1.125"  # replace with the current DHCP address
ESP32_PORT = 5000

with socket.create_connection((ESP32_IP, ESP32_PORT), timeout=10) as sock:
    print(f"Connected to {ESP32_IP}:{ESP32_PORT}")
    reader = sock.makefile("r", encoding="utf-8")
    for line in reader:
        print(line.rstrip())
```

Save as `receive_status.py` and run:

```powershell
python receive_status.py
```

If the laptop cannot connect, check that both devices are on the same hotspot, VPN is disabled, client/AP isolation is disabled, Windows Firewall permits the test program, and the ESP32 still has the expected DHCP address.

## Build, flash, and monitor

Use an ESP-IDF 5.x environment with the ESP32-C3 target selected.

Build only:

```powershell
idf.py set-target esp32c3
idf.py build
```

Flash and open the serial monitor:

```powershell
idf.py -p COM11 flash monitor
```

Replace `COM11` with the detected serial port. The serial monitor baud rate is 115200. Keep the sensor stationary during the startup calibration period. The application serial line contains the current axes, magnitudes, and one event status; system boot/Wi-Fi logs may also be emitted by ESP-IDF during startup.

## Configuration reference

Application settings are intentionally kept in `firmware/configs/app_config.h` rather than Kconfig:

| Setting | Current value | Purpose |
|---|---:|---|
| `APP_I2C_SDA_GPIO` | 8 | I²C data pin |
| `APP_I2C_SCL_GPIO` | 10 | I²C clock pin |
| `APP_IMU_I2C_ADDRESS` | `0x68` | IMU address |
| `APP_SAMPLE_PERIOD_MS` | 1000 | Serial output interval |
| `APP_CALIBRATION_SAMPLES` | 100 | Startup gyro samples |
| `APP_CALIBRATION_PERIOD_MS` | 20 | Calibration interval |
| `APP_FILTER_ALPHA` | 0.30 | Low-pass response |
| `APP_MOTION_EVAL_PERIOD_MS` | 100 | Internal evaluation interval |
| `APP_TCP_STATUS_PORT` | 5000 | Laptop TCP port |

All event-specific thresholds, confirmation counts, Wi-Fi credentials, and retry timing are also in this header.

## Limitations and safety notes

- This is a prototype, not a certified automotive safety system.
- Thresholds depend strongly on sensor mounting, vehicle orientation, road surface, and sensor quality; physical testing and tuning are required.
- The directional model assumes the configured X/Y/Z axes match forward, lateral, and vertical vehicle directions.
- A single stationary calibration cannot remove all accelerometer bias or compensate for an incorrectly mounted sensor.
- The current low-pass filter can attenuate very short impacts.
- Event labels are heuristic classifications, not severity measurements.
- `ROAD IMPACT` currently uses the configured Z gyro disturbance/recovery pattern; validate that the physical mounting produces a useful Z gyro response.
- Wi-Fi uses unencrypted, unauthenticated TCP on the local network. Do not expose port 5000 to the internet.
- The TCP service supports one laptop client at a time.
- DHCP addresses can change.
- Phone hotspots may isolate clients or use an incompatible band; ESP32-C3 Wi-Fi requires a compatible 2.4 GHz network.
- GPIO8 shares the onboard RGB LED and may require an alternate SDA pin.
- The laptop dashboard has not been implemented; the current laptop role is a raw TCP status receiver.
- There is no OTA update, TLS, authentication, data logging, crash classifier, or cloud service.

## Frozen checkpoints

The project has rollback snapshots in the workspace:

- `.checkpoint_1`: calibrated 2-second sensor reader
- `.checkpoint_2`: filtered and calibrated IMU output
- `.checkpoint_3`: noise-resistant motion detection
- `.checkpoint_4`: event classification with Z-gyro road-impact detection
- `.checkpoint_5`: frozen baseline including Wi-Fi/TCP status transport and this detailed documentation

Treat `.checkpoint_5` as the current frozen baseline. Make future changes in a separate revision and preserve this snapshot for rollback.
