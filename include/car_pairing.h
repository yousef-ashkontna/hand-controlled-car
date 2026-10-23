#ifndef CAR_PAIRING_H
#define CAR_PAIRING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static inline bool car_nonzero(const uint8_t *bytes, size_t size)
{
    for (size_t i = 0; i < size; ++i) if (bytes[i]) return true;
    return false;
}

static inline bool car_pairing_valid(const uint8_t sender[6], const uint8_t receiver[6],
                                    const uint8_t local[6], bool is_sender,
                                    const uint8_t pmk[16], const uint8_t lmk[16])
{
    return car_nonzero(sender, 6) && car_nonzero(receiver, 6) &&
           !(sender[0] & 1) && !(receiver[0] & 1) &&
           memcmp(sender, receiver, 6) != 0 &&
           memcmp(local, is_sender ? sender : receiver, 6) == 0 &&
           car_nonzero(pmk, 16) && car_nonzero(lmk, 16);
}

static inline bool car_peer_matches(const uint8_t *source, const uint8_t *destination,
                                    const uint8_t *peer, const uint8_t *local)
{
    return source && destination && peer && local &&
           memcmp(source, peer, 6) == 0 && memcmp(destination, local, 6) == 0;
}

#endif
