#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "car_control.h"

static void assert_stopped(car_output_t output)
{
    assert(output.left == 0 && output.right == 0);
    assert(output.touch_angle == 0 && output.tilt_angle == 90);
}

int main(void)
{
    car_sample_t sample = {.roll_cdeg = -1234, .pitch_cdeg = 5678, .touch = 1};
    uint8_t packet[CAR_PACKET_SIZE];
    const uint8_t golden[] = {1, 1, 0x2e, 0xfb, 0x2e, 0x16};
    assert(car_encode(&sample, packet));
    assert(memcmp(packet, golden, sizeof(golden)) == 0);
    car_sample_t decoded = {0};
    assert(car_decode(golden, sizeof(golden), &decoded));
    assert(decoded.roll_cdeg == -1234 && decoded.pitch_cdeg == 5678 && decoded.touch == 1);
    for (size_t length = 0; length < CAR_PACKET_SIZE; ++length)
        assert(!car_decode(packet, length, &decoded));
    assert(!car_decode(packet, CAR_PACKET_SIZE + 1, &decoded));
    assert(!car_decode(NULL, CAR_PACKET_SIZE, &decoded));
    packet[0] = 2;
    assert(!car_decode(packet, sizeof(packet), &decoded));
    packet[0] = 1;
    packet[1] = 2;
    assert(!car_decode(packet, sizeof(packet), &decoded));
    packet[1] = 1;
    packet[2] = 0xff;
    packet[3] = 0x7f;
    assert(!car_decode(packet, sizeof(packet), &decoded));
    assert(decoded.roll_cdeg == -1234); // Failed decoding must not replace the previous sample.
    sample.roll_cdeg = -9000;
    sample.pitch_cdeg = 9000;
    assert(car_encode(&sample, packet));
    assert(car_decode(packet, sizeof(packet), &decoded));
    sample.roll_cdeg = -9001;
    assert(!car_encode(&sample, packet));

    sample = (car_sample_t){.pitch_cdeg = 3000, .touch = 1};
    car_output_t output = car_control(&sample, 1000000, 1499999);
    assert(output.left > 0 && output.left == output.right);
    assert(output.touch_angle == 90 && output.tilt_angle == 120);
    assert_stopped(car_control(&sample, 1000000, 1500000)); // Exact timeout boundary.
    assert_stopped(car_control(&sample, 1000000, 2000000)); // Stale queue entry cannot restart motors.
    assert_stopped(car_control(&sample, -1, 0)); // No packet at boot.
    assert_stopped(car_control(&sample, 100, 99));
    assert_stopped(car_control(NULL, 0, 1));
    assert(car_control(&sample, 2000000, 2000001).left > 0); // Fresh reception recovers.
    sample = (car_sample_t){.roll_cdeg = 499, .pitch_cdeg = -499};
    output = car_control(&sample, 0, 1);
    assert(output.left == 0 && output.right == 0);
    sample = (car_sample_t){.roll_cdeg = 9000};
    output = car_control(&sample, 0, 1);
    assert(output.left == 255 && output.right == -255);
    sample = (car_sample_t){.pitch_cdeg = -9000};
    output = car_control(&sample, 0, 1);
    assert(output.left == -255 && output.right == -255 && output.tilt_angle == 0);
    int previous = 0;
    for (int pitch = 500; pitch <= 9000; ++pitch) {
        sample.pitch_cdeg = (int16_t)pitch;
        output = car_control(&sample, 0, 1);
        assert(output.left >= previous && output.left <= 255);
        previous = output.left;
    }
    puts("Protocol, motion mixing, and timeout regression tests passed");
    return 0;
}
