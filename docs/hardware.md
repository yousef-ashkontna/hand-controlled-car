# Hardware and wiring

These assignments are taken from the firmware. Verify the pin labels on your specific ESP32 board and peripheral modules.

## Sender

| Signal | ESP32 GPIO | Notes |
| --- | --- | --- |
| MPU6050 SDA | 21 | I2C data |
| MPU6050 SCL | 22 | I2C clock, configured for 400 kHz |
| Touch module output | 5 | Digital input; this is not ESP32 capacitive-touch sensing |
| MPU6050 AD0 | GND | Selects address `0x68` |

Use 3.3 V compatible logic and a common ground. Check the MPU6050 breakout's supply specification and I2C pull-ups. The code enables internal pull-ups; suitable external pull-ups may still be needed for reliable 400 kHz operation.

## Receiver

| Peripheral signal | ESP32 GPIO |
| --- | --- |
| Left motor IN1 | 15 |
| Left motor IN2 | 2 |
| Right motor IN3 | 4 |
| Right motor IN4 | 19 |
| Left motor ENA / PWM | 5 |
| Right motor ENB / PWM | 14 |
| Touch servo signal | 16 |
| Tilt servo signal | 17 |

Motor PWM uses 1 kHz and 8-bit duty values. Servos use 50 Hz with a configured 500-2500 microsecond pulse range; confirm that range is suitable for your servos before testing the endpoints.

Power motors through a suitable motor driver and supply, not from ESP32 GPIOs. Use an appropriately rated servo supply and connect its ground to the ESP32 and motor driver ground. The exact driver, power ratings, and mechanical assembly are not specified by this repository.

Some assigned ESP32 pins are boot-strapping pins. If the board fails to boot with peripherals connected, check whether the external circuit forces an incorrect boot level. GPIO16/17 availability also depends on the ESP32 module variant.

## Pairing

1. Build and flash each firmware without a pairing file. At 115200 baud it prints its **Station MAC**. The receiver keeps the motors stopped and its servos at their neutral commands.
2. From the project root, run the following command with the actual sender and receiver station MAC addresses (the values below are examples):

```sh
python scripts/configure_pairing.py --sender 02:12:34:56:78:9A --receiver 02:12:34:56:78:9B
```

3. The script creates ignored `include/pairing_config.h` with both addresses and random 16-byte PMK and LMK values. Keep this file local. It refuses to overwrite existing keys unless `--force` is supplied.
4. Run `pio run -t clean`, then rebuild and flash both boards from this project. Cleaning ensures the new optional header is picked up after a previous unconfigured build. Both boards must have the same keys. Clean, rebuild, and reflash both whenever keys change.
5. Confirm each board reports the expected station MAC. A role/MAC mismatch or missing configuration disables ESP-NOW operation.

Both peers use encrypted unicast on channel 1. The receiver also checks source and destination MACs, exact payload length, protocol version, touch state, and angle ranges before accepting a measurement. The project uses [ESP-IDF's ESP-NOW peer encryption](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/api-reference/network/esp_now.html#security).

## Hardware verification

With the wheels raised, check the following before driving:

- Confirm both boards boot with peripherals connected and sensor readings recover after a disconnected I2C cable.
- Check hand orientation, neutral motor stop, forward/reverse motion, and left/right steering.
- Confirm the touch servo moves between 0 and 90 degrees; the tilt servo follows pitch, with level at 90 degrees.
- Switch off the sender, disconnect its sensor, or interrupt the radio link. Verify motors stop after the last valid packet ages 500 ms, allowing the 20 ms control polling interval and scheduler latency.
- Verify stale or malformed traffic cannot keep the motors moving. Restore valid traffic and check recovery.
- Confirm servo endpoints do not bind mechanically, and verify supply stability with motors and servos running.

On timeout, the touch servo returns to 0 degrees and the tilt servo to 90 degrees. Software tests and successful builds do not substitute for these physical checks.
