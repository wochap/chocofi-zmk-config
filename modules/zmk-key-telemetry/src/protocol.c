/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#include <zmk_key_telemetry/protocol.h>

static void put_le16(uint8_t *output, uint16_t value) {
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8);
}

static void put_le32(uint8_t *output, uint32_t value) {
    for (size_t i = 0; i < sizeof(value); i++) {
        output[i] = (uint8_t)(value >> (i * 8U));
    }
}

static void put_le64(uint8_t *output, uint64_t value) {
    for (size_t i = 0; i < sizeof(value); i++) {
        output[i] = (uint8_t)(value >> (i * 8U));
    }
}

uint32_t zmk_key_telemetry_complete_layer_mask(uint32_t explicit_layers, uint8_t default_layer) {
    if (default_layer >= 32U) {
        return explicit_layers;
    }

    return explicit_layers | (UINT32_C(1) << default_layer);
}

void zmk_key_telemetry_encode(uint8_t output[ZMK_KEY_TELEMETRY_FRAME_SIZE],
                              const struct zmk_key_telemetry_frame *frame) {
    output[ZMK_KEY_TELEMETRY_OFFSET_VERSION] = ZMK_KEY_TELEMETRY_PROTOCOL_VERSION;
    output[ZMK_KEY_TELEMETRY_OFFSET_FLAGS] = frame->flags;
    put_le16(&output[ZMK_KEY_TELEMETRY_OFFSET_FRAME_SIZE], ZMK_KEY_TELEMETRY_FRAME_SIZE);
    put_le32(&output[ZMK_KEY_TELEMETRY_OFFSET_SEQUENCE], frame->sequence);
    put_le64(&output[ZMK_KEY_TELEMETRY_OFFSET_TIMESTAMP], frame->timestamp_ms);
    put_le64(&output[ZMK_KEY_TELEMETRY_OFFSET_POSITIONS], frame->pressed_positions);
    put_le32(&output[ZMK_KEY_TELEMETRY_OFFSET_LAYERS], frame->active_layers);
    put_le32(&output[ZMK_KEY_TELEMETRY_OFFSET_CHANGED_FIELDS], frame->changed_fields);
    put_le32(&output[ZMK_KEY_TELEMETRY_OFFSET_VALID_FIELDS], frame->valid_fields);
    output[ZMK_KEY_TELEMETRY_OFFSET_MODIFIERS] = frame->modifiers;
    output[ZMK_KEY_TELEMETRY_OFFSET_HID_INDICATORS] = frame->hid_indicators;
    output[ZMK_KEY_TELEMETRY_OFFSET_DEFAULT_LAYER] = frame->default_layer;
    output[ZMK_KEY_TELEMETRY_OFFSET_TRANSPORT] = frame->transport;
    output[ZMK_KEY_TELEMETRY_OFFSET_BLE_PROFILE] = frame->ble_profile;
    output[ZMK_KEY_TELEMETRY_OFFSET_CENTRAL_BATTERY] = frame->central_battery_pct;
    output[ZMK_KEY_TELEMETRY_OFFSET_PERIPHERAL_BATTERY] = frame->peripheral_battery_pct;
    output[ZMK_KEY_TELEMETRY_OFFSET_SPLIT_STATUS] = frame->split_status;
    put_le32(&output[ZMK_KEY_TELEMETRY_OFFSET_DROPPED_FRAMES], frame->dropped_frames);
}
