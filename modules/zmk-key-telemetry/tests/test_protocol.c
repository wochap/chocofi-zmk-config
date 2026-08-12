/* SPDX-License-Identifier: MIT */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <zmk_key_telemetry/protocol.h>

static void test_key_record_encoding(void) {
    const struct zmk_key_telemetry_record record = {
        .type = ZMK_KEY_TELEMETRY_MSG_KEY,
        .pressed = true,
        .position = 41,
        .sequence = 0x1234,
        .timestamp_ms = UINT32_C(0x89abcdef),
        .active_layers = UINT32_C(0x80000025),
        .pressed_positions = UINT64_C(0x0000030201000081),
    };
    const uint8_t expected[ZMK_KEY_TELEMETRY_RECORD_SIZE] = {
        0x01, 0x02, 0x01, 0x29, 0x34, 0x12, 0xef, 0xcd, 0xab, 0x89,
        0x25, 0x00, 0x00, 0x80, 0x81, 0x00, 0x00, 0x01, 0x02, 0x03,
    };
    uint8_t encoded[ZMK_KEY_TELEMETRY_RECORD_SIZE] = {0};

    zmk_key_telemetry_encode(encoded, &record);
    assert(memcmp(encoded, expected, sizeof(expected)) == 0);
}

static void test_snapshot_and_layer_serialization(void) {
    assert(zmk_key_telemetry_complete_layer_mask(0, 0) == UINT32_C(0x00000001));
    assert(zmk_key_telemetry_complete_layer_mask(UINT32_C(0x00000024), 0) == UINT32_C(0x00000025));
    assert(zmk_key_telemetry_complete_layer_mask(UINT32_C(0x80000000), 2) == UINT32_C(0x80000004));
    assert(zmk_key_telemetry_complete_layer_mask(UINT32_C(0x00000002), 32) == UINT32_C(0x00000002));

    const struct zmk_key_telemetry_record record = {
        .type = ZMK_KEY_TELEMETRY_MSG_SNAPSHOT,
        .position = ZMK_KEY_TELEMETRY_POSITION_NONE,
        .sequence = 7,
        .timestamp_ms = 1000,
        .active_layers =
            zmk_key_telemetry_complete_layer_mask((UINT32_C(1) << 2) | (UINT32_C(1) << 5), 0),
        .pressed_positions = (UINT64_C(1) << 1) | (UINT64_C(1) << 41),
    };
    uint8_t encoded[ZMK_KEY_TELEMETRY_RECORD_SIZE] = {0};

    zmk_key_telemetry_encode(encoded, &record);
    assert(encoded[ZMK_KEY_TELEMETRY_OFFSET_TYPE] == ZMK_KEY_TELEMETRY_MSG_SNAPSHOT);
    assert(encoded[ZMK_KEY_TELEMETRY_OFFSET_POSITION] == ZMK_KEY_TELEMETRY_POSITION_NONE);
    assert(encoded[ZMK_KEY_TELEMETRY_OFFSET_LAYERS] == 0x25);
    assert(encoded[ZMK_KEY_TELEMETRY_OFFSET_POSITIONS] == 0x02);
    assert(encoded[ZMK_KEY_TELEMETRY_OFFSET_POSITIONS + 5] == 0x02);
}

int main(void) {
    test_key_record_encoding();
    test_snapshot_and_layer_serialization();
    puts("protocol tests passed");
    return 0;
}
