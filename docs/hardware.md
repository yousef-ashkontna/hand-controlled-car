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

Obtain the receiver's station MAC from your board tooling or temporarily log it after Wi-Fi initialization with `esp_wifi_get_mac(WIFI_IF_STA, mac)`. Update `receiver_mac` in the sender. Both firmwares explicitly select Wi-Fi channel 1. ESP-NOW peer encryption is disabled in the sender.
