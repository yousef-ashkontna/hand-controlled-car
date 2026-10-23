# Firmware architecture

## Shared protocol

Both firmwares use the encoder and decoder in `src/common/car_control.c`, with the public interface in `include/car_control.h`. The payload is explicitly serialized so compiler padding and signed-integer layout cannot change the wire format.

| Offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 1 | Application protocol version, currently 1 |
| 1 | 1 | Touch state: 0 or 1 |
| 2 | 2 | Signed roll in hundredths of a degree, little endian |
| 4 | 2 | Signed pitch in hundredths of a degree, little endian |

The payload is exactly 6 bytes. Both angles must be within -9000 through +9000. Invalid length, version, state, or range causes rejection without updating the receiver's measurement or deadline. Both boards must run this protocol version.

The previously undefined vertical value is removed. The tilt servo uses measured pitch: -90 degrees maps to 0, level to 90, and +90 to 180.

## Sender

The sender initializes encrypted radio pairing, I2C, and the MPU6050 with checked return values. The driver checks allocation and pointer arguments, verifies WHO_AM_I, and explicitly sets +/-2 g and +/-250 degrees/second ranges.

Valid samples feed a complementary filter with 96% gyro estimate and 4% accelerometer estimate. Initial readings, recovery after read failures, and sample gaps over 100 ms reset the estimate from the accelerometer. atan2 and hypot avoid division by zero; an all-zero acceleration vector is rejected.

The sender transmits one measurement and waits for the send-completion notification before sending another, with a 20 ms delay between iterations. Read errors suspend transmission so the receiver can time out. Errors cannot send uninitialized sample variables or integrate the entire outage as a gyro interval.

## Receiver

The hardware starts with zero motor PWM. A one-slot measurement queue and an actuator task are created before the ESP-NOW receive callback is registered. Allocation and task-creation failures abort initialization.

The callback validates source/destination MACs and payload contents, stamps the valid measurement with monotonic arrival time, and overwrites the queue. It never writes actuators or a shared global timeout timestamp.

One actuator task owns all subsequent motor and servo writes. It uses the most recent measurement and its original arrival time, polling at most every 20 ms when there is no traffic. At an age of 500 ms it stops both motors and returns the servos to neutral. A queued measurement that is already stale cannot restart motion. Only a fresh valid packet can restore control.

Pitch controls forward/reverse speed; roll controls differential steering. Each uses a 5-degree dead zone. Mixed speeds are clamped to +/-255, then mapped monotonically through the minimum effective PWM of 90. PWM is disabled before motor direction pins change.

## Radio configuration

`src/common/car_radio.c` starts Wi-Fi station mode, prints the local MAC, and requires a valid local `include/pairing_config.h`. The pairing script generates private keys and both station addresses. Missing configuration, zero/multicast/identical addresses, a local role/MAC mismatch, or missing keys leaves ESP-NOW disabled.

Each board registers the other as an encrypted unicast peer using the same LMK, with an explicitly configured PMK. The receiver admits only its configured peer and local unicast destination. Broadcast control packets are rejected. See [pairing](hardware.md#pairing) and the [ESP-IDF ESP-NOW reference](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/api-reference/network/esp_now.html).

## Build and tests

PlatformIO supplies the firmware role to CMake as an ESP-IDF build property. Only that role's application entry point is compiled, alongside common control/radio code. The MPU6050 driver is an ESP-IDF component.

Host C regression tests cover golden wire bytes, signed values, malformed packets, dead zones, speed monotonicity, timeout boundaries, stale measurements, recovery, allocation failures, initialization failures, and failed sensor reads. GitHub Actions also builds both configured firmware roles with disposable keys and synthetic addresses. Local unconfigured builds exercise the configuration-required path. Hardware timing, radio delivery, and mechanics require the checks in [hardware.md](hardware.md#hardware-verification).
