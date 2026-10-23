#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_now.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_netif.h"

#include "driver/gpio.h"
#include "driver/ledc.h"

#define TAG "ROBOT_RCV"

// ================= MACROS & PINS =================
#define CLAMP(x,l,h) ((x)<(l)?(l):((x)>(h)?(h):(x)))

#define IN1 15
#define IN2 2
#define IN3 4
#define IN4 19
#define ENA 5
#define ENB 14

#define SERVO_TOUCH_PIN 16
#define SERVO_TILT_PIN 17

#define MOTOR_PWM_FREQ 1000
#define MOTOR_PWM_RES  LEDC_TIMER_8_BIT
#define SERVO_FREQ     50
#define SERVO_MIN_US   500
#define SERVO_MAX_US   2500
#define MIN_PWM        90

// ================= DATA STRUCTURES =================
// Matches the Transmitter's packed format
typedef struct __attribute__((packed)) {
    int16_t roll;     // Scaled by 100
    int16_t pitch;    // Scaled by 100
    int16_t vertical; // Scaled by 100
    uint8_t touch;
} incoming_packet_t;

typedef struct {
    float roll;
    float pitch;
    int touch;
    float vertical;
} sensor_data_t;

typedef struct {
    int leftSpeed;
    int rightSpeed;
} motor_cmd_t;

typedef struct {
    int touchAngle;
    int tiltAngle;
} servo_cmd_t;

// ================= GLOBAL WRAPPERS =================
QueueHandle_t sensorQueue;
QueueHandle_t motorQueue;
QueueHandle_t servoQueue;
uint64_t lastPacketTime = 0;

// ================= ESP-NOW CALLBACK =================
void espnow_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (len != sizeof(incoming_packet_t)) return;

    incoming_packet_t raw;
    memcpy(&raw, data, sizeof(raw));

    sensor_data_t decoded;
    // Reverse the scaling (divide by 100.0)
    decoded.roll     = raw.roll / 100.0f;
    decoded.pitch    = raw.pitch / 100.0f;
    decoded.vertical = raw.vertical / 100.0f;
    decoded.touch    = raw.touch;

    lastPacketTime = esp_timer_get_time() / 1000;

    // Send to processing task
    xQueueSend(sensorQueue, &decoded, 0);
}

// ================= HARDWARE INIT =================
void hardware_init() {
    // GPIO
    gpio_set_direction(IN1, GPIO_MODE_OUTPUT);
    gpio_set_direction(IN2, GPIO_MODE_OUTPUT);
    gpio_set_direction(IN3, GPIO_MODE_OUTPUT);
    gpio_set_direction(IN4, GPIO_MODE_OUTPUT);

    // Motor PWM (Timer 0)
    ledc_timer_config_t m_timer = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = MOTOR_PWM_FREQ,
        .duty_resolution = MOTOR_PWM_RES,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&m_timer);

    ledc_channel_config_t chA = {.gpio_num = ENA, .speed_mode = LEDC_HIGH_SPEED_MODE, .channel = LEDC_CHANNEL_0, .timer_sel = LEDC_TIMER_0, .duty = 0};
    ledc_channel_config_t chB = {.gpio_num = ENB, .speed_mode = LEDC_HIGH_SPEED_MODE, .channel = LEDC_CHANNEL_1, .timer_sel = LEDC_TIMER_0, .duty = 0};
    ledc_channel_config(&chA);
    ledc_channel_config(&chB);

    // Servo PWM (Timer 1)
    ledc_timer_config_t s_timer = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer_num = LEDC_TIMER_1,
        .freq_hz = SERVO_FREQ,
        .duty_resolution = LEDC_TIMER_16_BIT
    };
    ledc_timer_config(&s_timer);

    ledc_channel_config_t s1 = {.gpio_num = SERVO_TOUCH_PIN, .speed_mode = LEDC_HIGH_SPEED_MODE, .channel = LEDC_CHANNEL_2, .timer_sel = LEDC_TIMER_1};
    ledc_channel_config_t s2 = {.gpio_num = SERVO_TILT_PIN, .speed_mode = LEDC_HIGH_SPEED_MODE, .channel = LEDC_CHANNEL_3, .timer_sel = LEDC_TIMER_1};
    ledc_channel_config(&s1);
    ledc_channel_config(&s2);
}

// ================= HELPERS =================
void servo_write(int channel, int angle) {
    int pulse = SERVO_MIN_US + (SERVO_MAX_US - SERVO_MIN_US) * angle / 180;
    uint32_t duty = (pulse * 65535) / 20000;
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, channel, duty);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, channel);
}

int apply_min_pwm(int val) {
    if(val == 0) return 0;
    int sign = (val > 0) ? 1 : -1;
    int mag = abs(val);
    if(mag < MIN_PWM) mag = MIN_PWM + (mag * (255 - MIN_PWM) / 255);
    return sign * mag;
}

void setMotor(int left, int right) {
    left = apply_min_pwm(left);
    right = apply_min_pwm(right);

    gpio_set_level(IN1, left > 0);
    gpio_set_level(IN2, left < 0);
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, abs(left));
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);

    gpio_set_level(IN3, right > 0);
    gpio_set_level(IN4, right < 0);
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, abs(right));
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1);
}

// ================= TASKS =================
void motion_task(void *pv) {
    sensor_data_t data;
    motor_cmd_t motor;
    servo_cmd_t servo;
    const float K = 4.0;

    while(1) {
        if(xQueueReceive(sensorQueue, &data, portMAX_DELAY)) {
            // Deadzone
            if(fabs(data.pitch) < 5) data.pitch = 0;
            if(fabs(data.roll) < 5) data.roll = 0;

            int forward = (int)(K * data.pitch);
            int turn = (int)(K * data.roll);

            motor.leftSpeed = CLAMP(forward + turn, -255, 255);
            motor.rightSpeed = CLAMP(forward - turn, -255, 255);

            servo.touchAngle = data.touch ? 90 : 0;
            // Map vertical movement to tilt
            int tilt = (int)((data.vertical + 0.5) * 180);
            servo.tiltAngle = CLAMP(tilt, 0, 180);

            xQueueSend(motorQueue, &motor, 0);
            xQueueSend(servoQueue, &servo, 0);
        }
    }
}

void motor_task(void *pv) {
    motor_cmd_t cmd;
    while(1) {
        if(xQueueReceive(motorQueue, &cmd, portMAX_DELAY)) {
            setMotor(cmd.leftSpeed, cmd.rightSpeed);
        }
    }
}

void servo_task(void *pv) {
    servo_cmd_t s_cmd;
    while(1) {
        if(xQueueReceive(servoQueue, &s_cmd, portMAX_DELAY)) {
            servo_write(LEDC_CHANNEL_2, s_cmd.touchAngle);
            servo_write(LEDC_CHANNEL_3, s_cmd.tiltAngle);
        }
    }
}

void failsafe_task(void *pv) {
    while(1) {
        uint64_t now = esp_timer_get_time() / 1000;
        if(now - lastPacketTime > 500) {
            setMotor(0, 0); // Stop if no signal for 0.5s
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ================= MAIN =================
void app_main() {
    // 1. Storage & WiFi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE)); // Match Transmitter

    // 2. ESP-NOW
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));

    // 3. Hardware & Queues
    hardware_init();
    sensorQueue = xQueueCreate(5, sizeof(sensor_data_t));
    motorQueue  = xQueueCreate(5, sizeof(motor_cmd_t));
    servoQueue  = xQueueCreate(5, sizeof(servo_cmd_t));

    // 4. Tasks
    xTaskCreatePinnedToCore(motion_task, "motion", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(motor_task, "motor", 2048, NULL, 6, NULL, 0);
    xTaskCreatePinnedToCore(servo_task, "servo", 2048, NULL, 4, NULL, 0);
    xTaskCreatePinnedToCore(failsafe_task, "failsafe", 2048, NULL, 3, NULL, 0);

    ESP_LOGI(TAG, "Robot Receiver Initialized on Channel 1");
}
