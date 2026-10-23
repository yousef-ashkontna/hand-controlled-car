# Known issues and validation

This repository preserves the original application behavior. Publication cleanup fixes firmware selection and component registration, adds explicit standard/platform includes, and documents the project. It does not establish that the car works end to end.

## Issues found by source inspection

1. **Packet incompatibility:** the sender emits 5 bytes with whole-degree angles; the receiver expects 7 bytes, a `vertical` field, and values scaled by 100. The receiver rejects every current sender packet. Define the intended vertical measurement and a shared packet contract before pairing the boards.
2. **Receive startup ordering:** the receiver registers its ESP-NOW callback before creating its queues. A packet matching the receiver's expected size arriving during startup can reach an uninitialized queue.
3. **Motor ownership:** the failsafe task and motor task both write motor outputs, while the callback updates a shared timestamp. Motor writes and timeout handling should be consolidated or synchronized before relying on the failsafe.
4. **Sensor errors:** the sender does not check the MPU6050 read result before using sample variables. Allocation, initialization, and I2C errors also need stronger handling.
5. **Radio validation:** encryption is disabled, and the receiver validates length but does not authenticate the sender MAC. The sender's destination is a hard-coded device address.

## Hardware validation checklist

- [ ] Verify both boards boot with all peripherals attached.
- [ ] Confirm the MPU6050 address, orientation, and sensor calibration.
- [ ] Resolve and verify packet size, units, and field ordering on both boards.
- [ ] Confirm neutral hand position stops the motors and directions match the chassis.
- [ ] Verify servo endpoints without mechanical binding.
- [ ] Disconnect the transmitter and measure actual motor stop latency.
- [ ] Confirm no queued or concurrent command restarts the motors after a timeout.
- [ ] Test power stability while both motors and servos operate.

Build checks cover compilation and linking only. No physical hardware validation was performed during repository preparation.
