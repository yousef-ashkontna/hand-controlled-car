#include "mpu6050.h"
#include <stdlib.h>

#define MPU6050_PWR_MGMT_1   0x6B
#define MPU6050_ACCEL_XOUT_H 0x3B

static esp_err_t write_byte(i2c_port_t port, uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = {reg, data};

    return i2c_master_write_to_device(
        port,
        MPU6050_ADDR,
        buf,
        2,
        pdMS_TO_TICKS(100)
    );
}

static esp_err_t read_bytes(i2c_port_t port, uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(
        port,
        MPU6050_ADDR,
        &reg,
        1,
        data,
        len,
        pdMS_TO_TICKS(100)
    );
}

mpu6050_handle_t mpu6050_create(i2c_port_t port)
{
    mpu6050_handle_t dev = calloc(1, sizeof(mpu6050_t));
    dev->i2c_port = port;
    return dev;
}

esp_err_t mpu6050_init(mpu6050_handle_t dev)
{
    /* Wake up sensor */
    return write_byte(dev->i2c_port, MPU6050_PWR_MGMT_1, 0x00);
}

esp_err_t mpu6050_get_motion(
        mpu6050_handle_t dev,
        int16_t *ax,
        int16_t *ay,
        int16_t *az,
        int16_t *gx,
        int16_t *gy,
        int16_t *gz)
{
    uint8_t data[14];

    esp_err_t ret = read_bytes(
        dev->i2c_port,
        MPU6050_ACCEL_XOUT_H,
        data,
        14
    );

    if (ret != ESP_OK)
        return ret;

    *ax = (data[0] << 8) | data[1];
    *ay = (data[2] << 8) | data[3];
    *az = (data[4] << 8) | data[5];

    *gx = (data[8] << 8) | data[9];
    *gy = (data[10] << 8) | data[11];
    *gz = (data[12] << 8) | data[13];

    return ESP_OK;
}
