#ifndef CAR_CONTROL_H
#define CAR_CONTROL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CAR_PACKET_SIZE 6
#define CAR_PROTOCOL_VERSION 1
#define CAR_TIMEOUT_US INT64_C(500000)

typedef struct {
    int16_t roll_cdeg;
    int16_t pitch_cdeg;
    uint8_t touch;
} car_sample_t;

typedef struct {
    int left;
    int right;
    int touch_angle;
    int tilt_angle;
} car_output_t;

bool car_encode(const car_sample_t *sample, uint8_t packet[CAR_PACKET_SIZE]);
bool car_decode(const uint8_t *packet, size_t size, car_sample_t *sample);
car_output_t car_control(const car_sample_t *sample, int64_t received_us, int64_t now_us);

#endif
