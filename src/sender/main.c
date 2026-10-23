#include <math.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "car_control.h"
#include "car_radio.h"
#include "mpu6050.h"

#define TOUCH_PIN GPIO_NUM_5
static const char *TAG = "SENSOR_SEND";
static mpu6050_handle_t mpu;
static TaskHandle_t sender_task;

static void send_complete(const esp_now_send_info_t *info, esp_now_send_status_t status)
{
    (void)info;
    (void)status;
    xTaskNotifyGive(sender_task);
}

static float limit_angle(float angle)
{
    return fmaxf(-90.0f, fminf(90.0f, angle));
}

static void sensor_task(void *argument)
{
    (void)argument;
    sender_task = xTaskGetCurrentTaskHandle();
    ESP_ERROR_CHECK(esp_now_register_send_cb(send_complete));
    float roll = 0, pitch = 0;
    int64_t previous_us = 0;
    bool initialized = false;
    bool read_failed = false;
    while (true) {
        int16_t ax, ay, az, gx, gy, gz;
        esp_err_t result = mpu6050_get_motion(mpu, &ax, &ay, &az, &gx, &gy, &gz);
        int64_t now_us = esp_timer_get_time();
        if (result != ESP_OK || (ax == 0 && ay == 0 && az == 0)) {
            if (!read_failed) ESP_LOGE(TAG, "Invalid sensor sample; transmissions suspended");
            read_failed = true;
            initialized = false;
            // Invalid readings must not refresh the receiver timeout.
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        if (read_failed) ESP_LOGI(TAG, "Sensor readings recovered");
        read_failed = false;

        const float radians_to_degrees = 57.2957795f;
        float accel_roll = atan2f((float)ay, hypotf((float)ax, (float)az)) * radians_to_degrees;
        float accel_pitch = atan2f(-(float)ax, hypotf((float)ay, (float)az)) * radians_to_degrees;
        float dt = (now_us - previous_us) / 1000000.0f;
        previous_us = now_us;
        if (!initialized || dt <= 0 || dt > 0.1f) {
            roll = accel_roll;
            pitch = accel_pitch;
            initialized = true;
        } else {
            roll = limit_angle(0.96f * (roll + gx / 131.0f * dt) + 0.04f * accel_roll);
            pitch = limit_angle(0.96f * (pitch + gy / 131.0f * dt) + 0.04f * accel_pitch);
        }
        car_sample_t sample = {
            .roll_cdeg = (int16_t)lroundf(roll * 100),
            .pitch_cdeg = (int16_t)lroundf(pitch * 100),
            .touch = gpio_get_level(TOUCH_PIN) ? 1 : 0,
        };
        uint8_t packet[CAR_PACKET_SIZE];
        if (car_encode(&sample, packet)) {
            result = esp_now_send(car_radio_peer(), packet, sizeof(packet));
            if (result != ESP_OK) ESP_LOGW(TAG, "Send failed: %s", esp_err_to_name(result));
            else {
                // Never queue more measurements behind an unfinished radio send.
                // If the driver stops completing sends, the receiver times out.
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void app_main(void)
{
    if (!car_radio_init(true)) return;
    i2c_config_t i2c = {
        .mode = I2C_MODE_MASTER, .sda_io_num = 21, .scl_io_num = 22,
        .sda_pullup_en = GPIO_PULLUP_ENABLE, .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &i2c));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, i2c.mode, 0, 0, 0));
    gpio_config_t touch = {
        .pin_bit_mask = 1ULL << TOUCH_PIN, .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE, .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&touch));
    mpu = mpu6050_create(I2C_NUM_0);
    ESP_ERROR_CHECK(mpu ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(mpu6050_init(mpu));
    ESP_ERROR_CHECK(xTaskCreate(sensor_task, "sensor", 4096, NULL, 5, NULL) == pdPASS
                    ? ESP_OK : ESP_ERR_NO_MEM);
}
