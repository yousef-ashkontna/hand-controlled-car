#include "car_control.h"

static int clamp(int value, int low, int high)
{
    return value < low ? low : value > high ? high : value;
}

static bool valid(const car_sample_t *sample)
{
    return sample && sample->touch <= 1 &&
           sample->roll_cdeg >= -9000 && sample->roll_cdeg <= 9000 &&
           sample->pitch_cdeg >= -9000 && sample->pitch_cdeg <= 9000;
}

static int16_t read_i16(const uint8_t *bytes)
{
    unsigned value = (unsigned)bytes[0] | ((unsigned)bytes[1] << 8);
    return (int16_t)(value <= 32767 ? (int)value : (int)value - 65536);
}

bool car_encode(const car_sample_t *sample, uint8_t packet[CAR_PACKET_SIZE])
{
    if (!packet || !valid(sample)) return false;
    uint16_t roll = (uint16_t)sample->roll_cdeg;
    uint16_t pitch = (uint16_t)sample->pitch_cdeg;
    packet[0] = CAR_PROTOCOL_VERSION;
    packet[1] = sample->touch;
    packet[2] = (uint8_t)roll;
    packet[3] = (uint8_t)(roll >> 8);
    packet[4] = (uint8_t)pitch;
    packet[5] = (uint8_t)(pitch >> 8);
    return true;
}

bool car_decode(const uint8_t *packet, size_t size, car_sample_t *sample)
{
    if (!packet || !sample || size != CAR_PACKET_SIZE || packet[0] != CAR_PROTOCOL_VERSION) return false;
    car_sample_t decoded = {
        .roll_cdeg = read_i16(packet + 2),
        .pitch_cdeg = read_i16(packet + 4),
        .touch = packet[1],
    };
    if (!valid(&decoded)) return false;
    *sample = decoded;
    return true;
}

static int motor_pwm(int value)
{
    value = clamp(value, -255, 255);
    if (value == 0) return 0;
    int magnitude = value < 0 ? -value : value;
    // A monotonic mapping avoids the old speed drop at the minimum-PWM threshold.
    magnitude = 90 + (magnitude * (255 - 90) / 255);
    return value < 0 ? -magnitude : magnitude;
}

car_output_t car_control(const car_sample_t *sample, int64_t received_us, int64_t now_us)
{
    car_output_t output = {.tilt_angle = 90};
    if (!valid(sample) || received_us < 0 || now_us < received_us ||
        now_us - received_us >= CAR_TIMEOUT_US) return output;

    int pitch = sample->pitch_cdeg;
    int roll = sample->roll_cdeg;
    if (pitch > -500 && pitch < 500) pitch = 0;
    if (roll > -500 && roll < 500) roll = 0;
    int forward = pitch * 4 / 100;
    int turn = roll * 4 / 100;
    output.left = motor_pwm(forward + turn);
    output.right = motor_pwm(forward - turn);
    output.touch_angle = sample->touch ? 90 : 0;
    output.tilt_angle = clamp(90 + sample->pitch_cdeg / 100, 0, 180);
    return output;
}
