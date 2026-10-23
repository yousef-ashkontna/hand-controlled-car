#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "car_control.h"
#include "car_radio.h"

#define IN1 15
#define IN2 2
#define IN3 4
#define IN4 19
#define ENA 5
#define ENB 14
#define SERVO_TOUCH_PIN 16
#define SERVO_TILT_PIN 17

typedef struct {
    car_sample_t sample;
    int64_t received_us;
} received_sample_t;

static QueueHandle_t sample_queue;

static void receive_packet(const esp_now_recv_info_t *info, const uint8_t *data, int size)
{
    if (!car_radio_accepts(info) || size != CAR_PACKET_SIZE) return;
    received_sample_t received;
    if (!car_decode(data, (size_t)size, &received.sample)) return;
    received.received_us = esp_timer_get_time();
    // One slot holds the newest measurement, with its original arrival time.
    xQueueOverwrite(sample_queue, &received);
}

static void set_pwm(ledc_channel_t channel, uint32_t duty)
{
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_HIGH_SPEED_MODE, channel, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_HIGH_SPEED_MODE, channel));
}

static void set_motor(int positive_pin, int negative_pin, ledc_channel_t channel, int speed)
{
    // Remove enable PWM before changing direction.
    set_pwm(channel, 0);
    ESP_ERROR_CHECK(gpio_set_level(positive_pin, speed > 0));
    ESP_ERROR_CHECK(gpio_set_level(negative_pin, speed < 0));
    set_pwm(channel, (uint32_t)abs(speed));
}

static void set_servo(ledc_channel_t channel, int angle)
{
    uint32_t pulse_us = 500 + 2000 * angle / 180;
    set_pwm(channel, pulse_us * 65535 / 20000);
}

static void hardware_init(void)
{
    gpio_config_t outputs = {
        .pin_bit_mask = (1ULL << IN1) | (1ULL << IN2) | (1ULL << IN3) | (1ULL << IN4),
        .mode = GPIO_MODE_OUTPUT, .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&outputs));
    ESP_ERROR_CHECK(gpio_set_level(IN1, 0));
    ESP_ERROR_CHECK(gpio_set_level(IN2, 0));
    ESP_ERROR_CHECK(gpio_set_level(IN3, 0));
    ESP_ERROR_CHECK(gpio_set_level(IN4, 0));
    ledc_timer_config_t motors = {
        .speed_mode = LEDC_HIGH_SPEED_MODE, .timer_num = LEDC_TIMER_0,
        .freq_hz = 1000, .duty_resolution = LEDC_TIMER_8_BIT, .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config_t servos = {
        .speed_mode = LEDC_HIGH_SPEED_MODE, .timer_num = LEDC_TIMER_1,
        .freq_hz = 50, .duty_resolution = LEDC_TIMER_16_BIT, .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&motors));
    ESP_ERROR_CHECK(ledc_timer_config(&servos));
    const int pins[] = {ENA, ENB, SERVO_TOUCH_PIN, SERVO_TILT_PIN};
    for (int channel = 0; channel < 4; ++channel) {
        ledc_channel_config_t config = {
            .gpio_num = pins[channel], .speed_mode = LEDC_HIGH_SPEED_MODE,
            .channel = (ledc_channel_t)channel,
            .timer_sel = channel < 2 ? LEDC_TIMER_0 : LEDC_TIMER_1,
            .duty = 0, .hpoint = 0,
        };
        ESP_ERROR_CHECK(ledc_channel_config(&config));
    }
}

static void actuator_task(void *argument)
{
    (void)argument;
    received_sample_t latest = {.received_us = -1};
    car_output_t previous = {.left = -999, .right = -999, .touch_angle = -1, .tilt_angle = -1};
    while (true) {
        // The only task writing motors/servos also handles timeout decisions.
        car_output_t output = car_control(&latest.sample, latest.received_us, esp_timer_get_time());
        if (output.left != previous.left)
            set_motor(IN1, IN2, LEDC_CHANNEL_0, output.left);
        if (output.right != previous.right)
            set_motor(IN3, IN4, LEDC_CHANNEL_1, output.right);
        if (output.touch_angle != previous.touch_angle)
            set_servo(LEDC_CHANNEL_2, output.touch_angle);
        if (output.tilt_angle != previous.tilt_angle)
            set_servo(LEDC_CHANNEL_3, output.tilt_angle);
        previous = output;
        // Even with no radio traffic, check the age every 20 ms.
        xQueueReceive(sample_queue, &latest, pdMS_TO_TICKS(20));
    }
}

void app_main(void)
{
    hardware_init();
    sample_queue = xQueueCreate(1, sizeof(received_sample_t));
    ESP_ERROR_CHECK(sample_queue ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreate(actuator_task, "actuators", 4096, NULL, 6, NULL) == pdPASS
                    ? ESP_OK : ESP_ERR_NO_MEM);
    // Queue and actuator owner exist before reception begins.
    if (!car_radio_init(false)) return;
    ESP_ERROR_CHECK(esp_now_register_recv_cb(receive_packet));
}
