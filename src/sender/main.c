#include <stdio.h>
#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c.h"
#include "driver/gpio.h"

#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#include "esp_timer.h"
#include "esp_log.h"

#include "mpu6050.h"

#define TAG "SENSOR_SEND"

#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 400000

#define TOUCH_PIN GPIO_NUM_5

// Replace with your receiver MAC
static uint8_t receiver_mac[] = {0x80, 0xF3, 0xDA, 0x54, 0x0B, 0x9C};

typedef struct __attribute__((packed)) {
    int16_t roll;     // whole numbers only
    int16_t pitch;
    uint8_t touch;
} sensor_packet_t;

mpu6050_handle_t mpu;
sensor_packet_t packet;

static float roll = 0;
static float pitch = 0;
static int64_t prev_time = 0;

// ---------------- I2C INIT ----------------
void i2c_master_init() {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    ESP_LOGI(TAG, "I2C initialized");
}

// ---------------- ESP-NOW CALLBACK ----------------
void espnow_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    ESP_LOGI(TAG, "Received ESP-NOW packet, length: %d", len);
}

// ---------------- SENSOR TASK ----------------
void sensor_task(void *pv) {
    while (1) {
        int16_t ax, ay, az, gx, gy, gz;

        mpu6050_get_motion(mpu, &ax, &ay, &az, &gx, &gy, &gz);

        float AccX = ax / 16384.0;
        float AccY = ay / 16384.0;
        float AccZ = az / 16384.0;

        float GyroX = gx / 131.0;
        float GyroY = gy / 131.0;

        int64_t now = esp_timer_get_time();
        float dt = (now - prev_time) / 1000000.0;
        prev_time = now;

        float AccRoll  = atan(AccY / sqrt(AccX*AccX + AccZ*AccZ)) * 180 / M_PI;
        float AccPitch = atan(-AccX / sqrt(AccY*AccY + AccZ*AccZ)) * 180 / M_PI;

        roll  = 0.96 * (roll  + GyroX * dt) + 0.04 * AccRoll;
        pitch = 0.96 * (pitch + GyroY * dt) + 0.04 * AccPitch;

        int touchState = gpio_get_level(TOUCH_PIN);

        sensor_packet_t packet;
        packet.roll = (int16_t)roll;       // cast float to int if needed
        packet.pitch = (int16_t)pitch;
        packet.touch = touchState;

        // Debug print
        ESP_LOGI(TAG, "Roll: %d Pitch: %d Touch: %d ",
                 packet.roll, packet.pitch, packet.touch);

        esp_err_t send_status = esp_now_send(receiver_mac, (uint8_t *)&packet, sizeof(packet));
        if (send_status != ESP_OK) {
            ESP_LOGE(TAG, "Error sending ESP-NOW packet: %d", send_status);
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// ---------------- APP MAIN ----------------
void app_main(void) {
    ESP_LOGI(TAG, "Starting sensor sender...");

    // NVS init
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized");

    //  I2C init
    i2c_master_init();

    //  Touch GPIO
    gpio_set_direction(TOUCH_PIN, GPIO_MODE_INPUT);

    //  TCP/IP + Event loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    //  Default Wi-Fi STA
    esp_netif_create_default_wifi_sta();

    //  Wi-Fi init
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));
    ESP_LOGI(TAG, "Wi-Fi started");

    //  ESP-NOW init
    ESP_ERROR_CHECK(esp_now_init());
    esp_now_peer_info_t peerInfo = {0};
    memcpy(peerInfo.peer_addr, receiver_mac, 6);
    peerInfo.channel = 1;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK){
        ESP_LOGE(TAG, "Failed to add peer");
    }
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
    ESP_LOGI(TAG, "ESP-NOW initialized");

    //  MPU6050 init
    mpu = mpu6050_create(I2C_MASTER_NUM);
    mpu6050_init(mpu);
    prev_time = esp_timer_get_time();
    ESP_LOGI(TAG, "MPU6050 initialized");

    //  Create sensor task
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "Sensor task started");
}
