#pragma once
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
typedef int i2c_port_t;
#define I2C_NUM_0 0
#define I2C_NUM_MAX 2
#define pdMS_TO_TICKS(ms) (ms)
esp_err_t i2c_master_write_to_device(i2c_port_t port, uint8_t address,
    const uint8_t *data, size_t size, int timeout);
esp_err_t i2c_master_write_read_device(i2c_port_t port, uint8_t address,
    const uint8_t *write_data, size_t write_size, uint8_t *data, size_t size, int timeout);
