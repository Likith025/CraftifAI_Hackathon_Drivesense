
# Drivesense
> Real-time vehicle motion monitoring and event detection using ESP32, MPU6050, and FirmGen.
> Real-time vehicle motion monitoring and event detection using ESP32, MPU6050, and FirmGen.

## 1. Project Overview

Drive Sense is an embedded system designed to monitor vehicle motion using an MPU6050 inertial sensor connected to an ESP32.

The system reads accelerometer and gyroscope data, processes the data locally on the ESP32, and identifies significant motion events such as harsh acceleration/braking, aggressive cornering, and road impacts.

The firmware is developed iteratively using FirmGen. Natural-language requirements are used to generate, modify, build, deploy, and validate the firmware on real hardware.

The main idea is to perform the important processing at the edge:

```text
MPU6050
   ↓
ESP32
   ↓
Sensor Processing
   ↓
Event Detection
   ↓
Status
   ↓
Wi-Fi
   ↓
Laptop Dashboard

2. Problem Definition

Vehicle motion contains useful information about driving behavior and road conditions.

Sudden acceleration, hard braking, aggressive cornering, and road impacts can produce recognizable patterns in accelerometer and gyroscope data.

The objective of Drive Sense is to use a low-cost inertial sensor and ESP32 to process this motion data locally and identify significant events in real time.

Instead of sending raw sensor data to another system for processing, the ESP32 performs the sensor processing and event detection at the edge.

The resulting status can then be sent to a laptop over Wi-Fi for visualization.

The project also demonstrates an agentic firmware-development workflow using FirmGen, where firmware is developed through natural-language requirements and iteratively validated on physical hardware.

3. Target Users

Potential applications include:

Fleet operators
Commercial vehicle operators
Automotive developers
Vehicle telematics applications
Driver-behavior monitoring systems
Embedded and IoT applications

Drive Sense is currently a hackathon prototype and is not intended to be used as a certified automotive safety system.

4. Hardware
Main Components
Component	Purpose
ESP32	Main MCU and edge-processing device
MPU6050	3-axis accelerometer and 3-axis gyroscope
USB cable	Power and firmware programming
Jumper wires	Sensor connections
Laptop	FirmGen development and dashboard
Logic analyzer	I²C and digital-signal debugging

The MPU6050 communicates with the ESP32 using I²C.

5. System Architecture
                    ┌──────────────────┐
                    │      MPU6050     │
                    │                  │
                    │ Accelerometer    │
                    │ Gyroscope        │
                    └────────┬─────────┘
                             │
                             │ I²C
                             ▼
                    ┌──────────────────┐
                    │      ESP32       │
                    │                  │
                    │ Sensor Reading   │
                    │ Calibration      │
                    │ Filtering        │
                    │ Motion Detection │
                    │ Event Detection  │
                    └────────┬─────────┘
                             │
                             │ Wi-Fi / TCP
                             ▼
                    ┌──────────────────┐
                    │ Laptop Dashboard │
                    │                  │
                    │ Current Status   │
                    └──────────────────┘

The ESP32 performs the primary sensor processing and event detection.

The laptop dashboard only displays the resulting status.

6. Sensor Data

The MPU6050 provides six motion measurements.

Accelerometer
X-axis acceleration
Y-axis acceleration
Z-axis acceleration

Unit:

g

The firmware also calculates acceleration magnitude.

Gyroscope
X-axis angular velocity
Y-axis angular velocity
Z-axis angular velocity

Unit:

degrees per second (dps)

The firmware also calculates angular velocity magnitude.

7. Sensor Orientation

For the current implementation, the sensor is interpreted as:

X → Forward / Backward
Y → Left / Right
Z → Vertical

This orientation is used when interpreting the sensor data for vehicle-related events.

The sensor should remain mounted in a consistent orientation during testing.

8. Firmware Development

The firmware was developed incrementally using FirmGen.

The development process follows:

Natural-language requirement
          ↓
FirmGen generated plan
          ↓
Firmware generation
          ↓
Build
          ↓
Flash to ESP32
          ↓
Test on real hardware
          ↓
Observe results
          ↓
Refine requirement
          ↓
Rebuild and validate

The firmware development was performed directly against real MPU6050 data.

9. Sensor Acquisition

The first stage of development established communication between the ESP32 and MPU6050 using I²C.

The firmware reads:

Accelerometer X/Y/Z
Gyroscope X/Y/Z

Example output:

ACC [g] X:+0.156 Y:+0.926 Z:+0.369 |
GYRO [dps] X:-1.588 Y:-3.557 Z:+0.374

This stage was used to verify that the sensor was communicating correctly and producing usable measurements.

10. Calibration and Filtering

At startup, the firmware collects stationary samples from the MPU6050.

The collected samples are used to estimate sensor offsets.

The firmware then:

Collects stationary samples.
Calculates accelerometer and gyroscope offsets.
Applies gyroscope bias correction.
Applies a basic low-pass filter.
Calculates acceleration magnitude.
Calculates angular velocity magnitude.
Continues outputting the processed values.

Example:

ACC [g] X:-0.017 Y:+0.133 Z:+1.008 MAG:+1.017 |
GYRO [dps] X:+0.013 Y:-0.108 Z:-0.202 MAG:+0.229

11. Event Detection

After basic motion detection was validated, the firmware was extended to classify significant movement.

The planned event types are:

HARSH ACCEL/BRAKING
AGGRESSIVE CORNERING
ROAD IMPACT
Harsh Acceleration / Braking

Uses the longitudinal acceleration component to identify significant forward or backward acceleration.

X-axis → Forward / Backward
Aggressive Cornering

Uses lateral acceleration together with rotation around the vertical axis.

Y-axis acceleration
+
Z-axis gyro rotation
Road Impact

Looks for a sudden vertical acceleration disturbance followed by recovery within a short time window.

Sudden Z-axis disturbance
        ↓
Short recovery window
        ↓
ROAD IMPACT

The detection logic uses multiple samples so that a single noisy sensor reading does not immediately classify an event.

13. Event Priority

Only one event status is reported at a time.

The priority is:

ROAD IMPACT
     ↓
AGGRESSIVE CORNERING
     ↓
HARSH ACCEL/BRAKING
     ↓
MOTION DETECTED
     ↓
NORMAL

If multiple conditions overlap, the higher-priority event is reported.

This keeps the dashboard simple and prevents multiple conflicting status labels from being displayed simultaneously.

12. Detection Parameters

The initial prototype values used during development are:

Harsh acceleration/braking:
0.30 g, confirmed over 2 samples

Cornering:
0.25 g lateral acceleration
+
12 dps Z-axis rotation

Road impact:
0.35 g Z-axis disturbance
+
0.15 g sudden change

Impact recovery window:
500 ms

Event hold time:
1000 ms

Return-to-normal confirmation:
3 quiet evaluations

These values are prototype thresholds and were intended for demonstration and hardware tuning rather than production automotive use.

13. Serial Output

The firmware provides serial output for development and validation.

The current format is:

ACC [g] X:<value> Y:<value> Z:<value> MAG:<value> |
GYRO [dps] X:<value> Y:<value> Z:<value> MAG:<value> |
STATUS:<event>

Example:

ACC [g] X:-0.097 Y:-0.008 Z:+1.018 MAG:+1.022 |
GYRO [dps] X:-0.176 Y:+0.001 Z:-0.130 MAG:+0.219 |
STATUS:NORMAL

14. Wi-Fi Communication

The ESP32 connects to a Wi-Fi network as a station.

The laptop and ESP32 must be connected to the same Wi-Fi network.

The ESP32 runs a TCP server on a configurable port.

The communication architecture is:

ESP32
  │
  │ Wi-Fi
  ▼
TCP Server
  │
  ▼
Laptop

The ESP32 remains responsible for all sensor processing and event decisions.

The laptop receives the resulting status.

The Wi-Fi credentials are kept separately from the sensor logic in the application configuration.

Example configuration:

WIFI_SSID
WIFI_PASSWORD
TCP_PORT

The TCP connection supports a laptop client connecting to the ESP32.

If the laptop disconnects, the ESP32 continues running the sensor and event-detection logic.

15. TCP Status Communication

The ESP32 sends the current system status to the connected laptop.

The status values include:

NORMAL
MOTION DETECTED
HARSH ACCEL/BRAKING
AGGRESSIVE CORNERING
ROAD IMPACT

The communication is intentionally simple so that the laptop dashboard only needs to receive the latest status.

Example:

NORMAL

or:

ROAD IMPACT

The TCP server uses a configurable port

Wiring

The MPU6050 communicates with the ESP32 using I²C.

MPU6050	ESP32	Function
VCC	[UPDATE]	Power
GND	GND	Ground
SDA	[UPDATE]	I²C Data
SCL	[UPDATE]	I²C Clock

Update the pin numbers with the final hardware configuration before submission.

Bill of Materials
Component	Quantity	Purpose
ESP32	1	Main MCU and edge processing
MPU6050	1	Accelerometer and gyroscope
USB cable	1	Power and programming
Jumper wires	As required	Hardware connections
Laptop	1	FirmGen and dashboard

Limitations
The event-detection thresholds are prototype values.
MPU6050 measurements are affected by sensor noise and mounting orientation.
Event detection is rule-based and requires further real-world validation.
The system has not been validated against real vehicle or crash datasets.
Wi-Fi is required for communication with the laptop dashboard.
The prototype is not intended for safety-critical automotive applications.
The current implementation is designed for hackathon demonstration rather than production deployment.
