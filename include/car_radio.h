#ifndef CAR_RADIO_H
#define CAR_RADIO_H

#include <stdbool.h>
#include "esp_now.h"

// Starts Wi-Fi and logs the local STA MAC even when pairing is not configured.
// Returns false without enabling ESP-NOW if local pairing is absent or invalid.
bool car_radio_init(bool sender);
bool car_radio_accepts(const esp_now_recv_info_t *info);
const uint8_t *car_radio_peer(void);

#endif
