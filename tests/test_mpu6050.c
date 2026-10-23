#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool allocation_fails;
static void *test_calloc(size_t count, size_t size)
{
    return allocation_fails ? NULL : calloc(count, size);
}
#define calloc test_calloc
#include "../components/mpu6050/mpu6050.c"
#undef calloc

static bool read_fails;
static int write_count;
static int fail_write;
static uint8_t who_am_i = 0x68;
static uint8_t registers[256];

void vTaskDelay(int ticks) { assert(ticks == 100); }

esp_err_t i2c_master_write_to_device(i2c_port_t port, uint8_t address,
    const uint8_t *data, size_t size, int timeout)
{
    assert(port == I2C_NUM_0 && address == 0x68 && size == 2 && timeout == 100);
    if (++write_count == fail_write) return ESP_FAIL;
    registers[data[0]] = data[1];
    return ESP_OK;
}

esp_err_t i2c_master_write_read_device(i2c_port_t port, uint8_t address,
    const uint8_t *write_data, size_t write_size, uint8_t *data, size_t size, int timeout)
{
    assert(port == I2C_NUM_0 && address == 0x68 && write_size == 1 && timeout == 100);
    if (read_fails) return ESP_FAIL;
    if (write_data[0] == 0x75) {
        assert(size == 1);
        data[0] = who_am_i;
    } else {
        const uint8_t sample[] = {0x80, 0, 0x7f, 0xff, 0, 1, 0, 0, 0xff, 0xff, 0, 2, 0, 3};
        assert(write_data[0] == 0x3b && size == sizeof(sample));
        memcpy(data, sample, size);
    }
    return ESP_OK;
}

int main(void)
{
    assert(mpu6050_create(-1) == NULL);
    assert(mpu6050_create(I2C_NUM_MAX) == NULL);
    allocation_fails = true;
    assert(mpu6050_create(I2C_NUM_0) == NULL);
    allocation_fails = false;
    mpu6050_handle_t device = mpu6050_create(I2C_NUM_0);
    assert(device);
    assert(mpu6050_init(NULL) == ESP_ERR_INVALID_ARG);
    read_fails = true;
    assert(mpu6050_init(device) == ESP_FAIL && write_count == 0);
    read_fails = false;
    who_am_i = 0;
    assert(mpu6050_init(device) == ESP_ERR_NOT_FOUND && write_count == 0);
    who_am_i = 0x68;
    for (fail_write = 1; fail_write <= 4; ++fail_write) {
        write_count = 0;
        assert(mpu6050_init(device) == ESP_FAIL);
        assert(write_count == fail_write);
    }
    fail_write = 0;
    write_count = 0;
    assert(mpu6050_init(device) == ESP_OK && write_count == 4);
    assert(registers[0x6b] == 1 && registers[0x1b] == 0 && registers[0x1c] == 0 && registers[0x1a] == 3);
    int16_t ax = 99, ay = 99, az = 99, gx = 99, gy = 99, gz = 99;
    read_fails = true;
    assert(mpu6050_get_motion(device, &ax, &ay, &az, &gx, &gy, &gz) == ESP_FAIL);
    assert(ax == 99 && ay == 99 && az == 99 && gx == 99 && gy == 99 && gz == 99);
    read_fails = false;
    assert(mpu6050_get_motion(device, NULL, &ay, &az, &gx, &gy, &gz) == ESP_ERR_INVALID_ARG);
    assert(mpu6050_get_motion(NULL, &ax, &ay, &az, &gx, &gy, &gz) == ESP_ERR_INVALID_ARG);
    assert(mpu6050_get_motion(device, &ax, &ay, &az, &gx, &gy, &gz) == ESP_OK);
    assert(ax == -32768 && ay == 32767 && az == 1 && gx == -1 && gy == 2 && gz == 3);
    free(device);
    puts("Sensor allocation, initialization, and read-failure regression tests passed");
    return 0;
}
