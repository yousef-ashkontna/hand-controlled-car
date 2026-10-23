#include <assert.h>
#include <stdio.h>
#include "car_pairing.h"

int main(void)
{
    const uint8_t sender[6] = {2, 0, 0, 0, 0, 1};
    const uint8_t receiver[6] = {2, 0, 0, 0, 0, 2};
    const uint8_t unknown[6] = {2, 0, 0, 0, 0, 3};
    const uint8_t broadcast[6] = {255, 255, 255, 255, 255, 255};
    const uint8_t zero[16] = {0};
    const uint8_t key[16] = {1}; // Test fixture only.
    assert(car_pairing_valid(sender, receiver, sender, true, key, key));
    assert(car_pairing_valid(sender, receiver, receiver, false, key, key));
    assert(!car_pairing_valid(sender, receiver, receiver, true, key, key));
    assert(!car_pairing_valid(sender, receiver, unknown, false, key, key));
    assert(!car_pairing_valid(sender, sender, sender, true, key, key));
    assert(!car_pairing_valid(zero, receiver, zero, true, key, key));
    assert(!car_pairing_valid(sender, broadcast, sender, true, key, key));
    assert(!car_pairing_valid(sender, receiver, sender, true, zero, key));
    assert(!car_pairing_valid(sender, receiver, sender, true, key, zero));
    assert(car_peer_matches(sender, receiver, sender, receiver));
    assert(!car_peer_matches(unknown, receiver, sender, receiver));
    assert(!car_peer_matches(sender, broadcast, sender, receiver));
    assert(!car_peer_matches(NULL, receiver, sender, receiver));
    assert(!car_peer_matches(sender, NULL, sender, receiver));
    assert(!car_peer_matches(sender, receiver, NULL, receiver));
    puts("Pairing configuration and peer filtering regression tests passed");
    return 0;
}
