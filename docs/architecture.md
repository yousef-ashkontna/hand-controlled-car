# Firmware architecture

## Sender

`src/sender/main.c` initializes NVS, I2C, the touch input, Wi-Fi station mode, ESP-NOW, and the MPU6050. Its sensor task reads accelerometer and gyro samples, applies a complementary filter (96% gyro estimate and 4% accelerometer estimate), and sends roll, pitch, and touch state. It delays 20 ms between iterations; processing time adds to that interval.

`components/mpu6050/` contains a small ESP-IDF I2C driver. It wakes the device and reads the accelerometer and gyroscope registers. Conversion assumes the default +/-2 g and +/-250 degrees/second ranges.

## Receiver

`src/receiver/main.c` defines these paths:

| Task/callback | Responsibility |
| --- | --- |
| ESP-NOW receive callback | Validate packet length, decode fields, update reception time, enqueue sensor data |
| Motion task | Apply dead zones; mix pitch and roll into left/right speeds; compute servo commands |
| Motor task | Apply direction and PWM commands |
| Servo task | Apply touch and tilt servo angles |
| Failsafe task | Check every 100 ms and request a motor stop when packet age exceeds 500 ms |

The three queues hold five elements each. Motor commands are clamped to -255 through 255. Touch selects 0 or 90 degrees. Tilt maps the receiver's `vertical` value to 0-180 degrees.

## Current wire formats

The source currently contains **two incompatible packed structs**:

| Firmware | Fields in order | Size | Interpretation |
| --- | --- | --- | --- |
| Sender | `int16_t roll`, `int16_t pitch`, `uint8_t touch` | 5 bytes | Whole-degree angles |
| Receiver | `int16_t roll`, `int16_t pitch`, `int16_t vertical`, `uint8_t touch` | 7 bytes | Three values divided by 100 |

The receiver rejects the sender's packets because their lengths differ. Simply changing the size check would not fix the field order or scaling. Agree on units and the meaning of `vertical`, then define a shared protocol header used by both targets. See [known issues](known-issues.md).

## Build organization

PlatformIO supplies `FIRMWARE_ROLE` to the root CMake project. The project stores it as an ESP-IDF build property so component dependency discovery receives the same value. `src/CMakeLists.txt` registers only that role's source file and dependencies. The sensor driver uses the standard ESP-IDF `components/` discovery mechanism.

References: [PlatformIO ESP-IDF integration](https://docs.platformio.org/en/latest/frameworks/espidf.html) and [Espressif32 configuration](https://docs.platformio.org/en/latest/platforms/espressif32.html).
