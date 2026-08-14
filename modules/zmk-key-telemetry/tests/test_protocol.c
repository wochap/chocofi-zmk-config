/* SPDX-License-Identifier: MIT */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <zmk_key_telemetry/protocol.h>

static void test_protocol_constants(void) {
    assert(ZMK_KEY_TELEMETRY_PROTOCOL_VERSION == 2U);
    assert(ZMK_KEY_TELEMETRY_FRAME_SIZE == 48U);
    assert(ZMK_KEY_TELEMETRY_POSITION_COUNT == 64U);
    assert(ZMK_KEY_TELEMETRY_KNOWN_FLAGS == 0x01U);
    assert(ZMK_KEY_TELEMETRY_KNOWN_FIELDS == 0x000001ffU);
    assert(ZMK_KEY_TELEMETRY_OFFSET_VERSION == 0U);
    assert(ZMK_KEY_TELEMETRY_OFFSET_FRAME_SIZE == 2U);
    assert(ZMK_KEY_TELEMETRY_OFFSET_SEQUENCE == 4U);
    assert(ZMK_KEY_TELEMETRY_OFFSET_TIMESTAMP == 8U);
    assert(ZMK_KEY_TELEMETRY_OFFSET_POSITIONS == 16U);
    assert(ZMK_KEY_TELEMETRY_OFFSET_LAYERS == 24U);
    assert(ZMK_KEY_TELEMETRY_OFFSET_CHANGED_FIELDS == 28U);
    assert(ZMK_KEY_TELEMETRY_OFFSET_VALID_FIELDS == 32U);
    assert(ZMK_KEY_TELEMETRY_OFFSET_MODIFIERS == 36U);
    assert(ZMK_KEY_TELEMETRY_OFFSET_DROPPED_FRAMES == 44U);
    assert(ZMK_KEY_TELEMETRY_TRANSPORT_UNKNOWN == 0U);
    assert(ZMK_KEY_TELEMETRY_TRANSPORT_USB == 1U);
    assert(ZMK_KEY_TELEMETRY_TRANSPORT_BLE == 2U);
    assert(ZMK_KEY_TELEMETRY_SPLIT_UNKNOWN == 0U);
    assert(ZMK_KEY_TELEMETRY_SPLIT_DISCONNECTED == 1U);
    assert(ZMK_KEY_TELEMETRY_SPLIT_CONNECTED == 2U);
}

static void test_state_frame_encoding(void) {
    const struct zmk_key_telemetry_frame frame = {
        .flags = ZMK_KEY_TELEMETRY_FLAG_SNAPSHOT,
        .sequence = UINT32_C(0x12345678),
        .timestamp_ms = UINT64_C(0x0123456789abcdef),
        .pressed_positions = UINT64_C(0x8000000801000081),
        .active_layers = UINT32_C(0x80000025),
        .changed_fields = ZMK_KEY_TELEMETRY_KNOWN_FIELDS,
        .valid_fields = UINT32_C(0x000001f7),
        .modifiers = UINT8_C(0xa5),
        .hid_indicators = UINT8_C(0x03),
        .default_layer = 2,
        .transport = ZMK_KEY_TELEMETRY_TRANSPORT_BLE,
        .ble_profile = 4,
        .central_battery_pct = 99,
        .peripheral_battery_pct = 87,
        .split_status = ZMK_KEY_TELEMETRY_SPLIT_CONNECTED,
        .dropped_frames = UINT32_C(0x89abcdef),
    };
    const uint8_t expected[ZMK_KEY_TELEMETRY_FRAME_SIZE] = {
        0x02, 0x01, 0x30, 0x00, 0x78, 0x56, 0x34, 0x12,
        0xef, 0xcd, 0xab, 0x89, 0x67, 0x45, 0x23, 0x01,
        0x81, 0x00, 0x00, 0x01, 0x08, 0x00, 0x00, 0x80,
        0x25, 0x00, 0x00, 0x80, 0xff, 0x01, 0x00, 0x00,
        0xf7, 0x01, 0x00, 0x00, 0xa5, 0x03, 0x02, 0x02,
        0x04, 0x63, 0x57, 0x02, 0xef, 0xcd, 0xab, 0x89,
    };
    uint8_t encoded[ZMK_KEY_TELEMETRY_FRAME_SIZE] = {0};

    zmk_key_telemetry_encode(encoded, &frame);
    assert(memcmp(encoded, expected, sizeof(expected)) == 0);
}

static void test_unknown_optional_state_and_boundaries(void) {
    const struct zmk_key_telemetry_frame frame = {
        .sequence = UINT32_MAX,
        .timestamp_ms = UINT64_MAX,
        .pressed_positions = UINT64_MAX,
        .active_layers = UINT32_MAX,
        .changed_fields = ZMK_KEY_TELEMETRY_FIELD_POSITIONS,
        .valid_fields = ZMK_KEY_TELEMETRY_FIELD_POSITIONS | ZMK_KEY_TELEMETRY_FIELD_LAYERS |
                        ZMK_KEY_TELEMETRY_FIELD_MODIFIERS |
                        ZMK_KEY_TELEMETRY_FIELD_DEFAULT_LAYER,
        .default_layer = 31,
        .transport = ZMK_KEY_TELEMETRY_TRANSPORT_UNKNOWN,
        .ble_profile = ZMK_KEY_TELEMETRY_VALUE_UNKNOWN,
        .central_battery_pct = ZMK_KEY_TELEMETRY_VALUE_UNKNOWN,
        .peripheral_battery_pct = ZMK_KEY_TELEMETRY_VALUE_UNKNOWN,
        .split_status = ZMK_KEY_TELEMETRY_SPLIT_UNKNOWN,
        .dropped_frames = UINT32_MAX,
    };
    uint8_t encoded[ZMK_KEY_TELEMETRY_FRAME_SIZE] = {0};

    zmk_key_telemetry_encode(encoded, &frame);
    assert(encoded[ZMK_KEY_TELEMETRY_OFFSET_FLAGS] == 0U);
    assert(encoded[ZMK_KEY_TELEMETRY_OFFSET_BLE_PROFILE] == UINT8_MAX);
    assert(encoded[ZMK_KEY_TELEMETRY_OFFSET_CENTRAL_BATTERY] == UINT8_MAX);
    assert(encoded[ZMK_KEY_TELEMETRY_OFFSET_PERIPHERAL_BATTERY] == UINT8_MAX);
    for (size_t i = ZMK_KEY_TELEMETRY_OFFSET_TIMESTAMP;
         i < ZMK_KEY_TELEMETRY_OFFSET_TIMESTAMP + sizeof(uint64_t); i++) {
        assert(encoded[i] == UINT8_MAX);
    }
}

static void test_complete_layer_mask(void) {
    assert(zmk_key_telemetry_complete_layer_mask(0, 0) == UINT32_C(0x00000001));
    assert(zmk_key_telemetry_complete_layer_mask(UINT32_C(0x00000024), 0) ==
           UINT32_C(0x00000025));
    assert(zmk_key_telemetry_complete_layer_mask(UINT32_C(0x80000000), 2) ==
           UINT32_C(0x80000004));
    assert(zmk_key_telemetry_complete_layer_mask(UINT32_C(0x00000002), 32) ==
           UINT32_C(0x00000002));
}

int main(void) {
    test_protocol_constants();
    test_state_frame_encoding();
    test_unknown_optional_state_and_boundaries();
    test_complete_layer_mask();
    puts("protocol v2 tests passed");
    return 0;
}
