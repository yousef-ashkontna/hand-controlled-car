#ifndef MPU6050_H
#define MPU6050_H

#include "driver/i2c.h"
#include "esp_err.h"

#define MPU6050_ADDR 0x68

typedef struct {
    i2c_port_t i2c_port;
} mpu6050_t;

typedef mpu6050_t* mpu6050_handle_t;

/* Create device */
mpu6050_handle_t mpu6050_create(i2c_port_t port);

/* Initialize sensor */
esp_err_t mpu6050_init(mpu6050_handle_t dev);

/* Read accelerometer and gyro */
esp_err_t mpu6050_get_motion(
        mpu6050_handle_t dev,
        int16_t *ax,
        int16_t *ay,
        int16_t *az,
        int16_t *gx,
        int16_t *gy,
        int16_t *gz);

#endif
