#include "car_radio.h"
#include "car_pairing.h"

#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#if __has_include("pairing_config.h")
#include "pairing_config.h"
#else
#define CAR_SENDER_MAC {0}
#define CAR_RECEIVER_MAC {0}
#define CAR_PMK {0}
#define CAR_LMK {0}
#endif

static const uint8_t sender_mac[6] = CAR_SENDER_MAC;
static const uint8_t receiver_mac[6] = CAR_RECEIVER_MAC;
static const uint8_t pmk[ESP_NOW_KEY_LEN] = CAR_PMK;
static const uint8_t lmk[ESP_NOW_KEY_LEN] = CAR_LMK;
static uint8_t local_mac[6];
static const uint8_t *peer_mac;
static const char *TAG = "CAR_RADIO";

const uint8_t *car_radio_peer(void)
{
    return peer_mac;
}

bool car_radio_accepts(const esp_now_recv_info_t *info)
{
    return info && car_peer_matches(info->src_addr, info->des_addr, peer_mac, local_mac);
}

bool car_radio_init(bool sender)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_netif_create_default_wifi_sta() ? ESP_OK : ESP_ERR_NO_MEM);
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&config));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, local_mac));
    ESP_LOGI(TAG, "Station MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             local_mac[0], local_mac[1], local_mac[2], local_mac[3], local_mac[4], local_mac[5]);

    const uint8_t *expected_peer = sender ? receiver_mac : sender_mac;
    if (!car_pairing_valid(sender_mac, receiver_mac, local_mac, sender, pmk, lmk)) {
        ESP_LOGE(TAG, "Pairing missing or incorrect. Run scripts/configure_pairing.py with both station MACs, then rebuild both boards.");
        return false;
    }

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_set_pmk(pmk));
    esp_now_peer_info_t peer = {.channel = 1, .ifidx = WIFI_IF_STA, .encrypt = true};
    memcpy(peer.peer_addr, expected_peer, sizeof(peer.peer_addr));
    memcpy(peer.lmk, lmk, sizeof(peer.lmk));
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));
    peer_mac = expected_peer;
    return true;
}
