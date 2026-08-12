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
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8);
    output[2] = (uint8_t)(value >> 16);
    output[3] = (uint8_t)(value >> 24);
}

uint32_t zmk_key_telemetry_complete_layer_mask(uint32_t explicit_layers, uint8_t default_layer) {
    if (default_layer >= 32U) {
        return explicit_layers;
    }

    return explicit_layers | (UINT32_C(1) << default_layer);
}

void zmk_key_telemetry_encode(uint8_t output[ZMK_KEY_TELEMETRY_RECORD_SIZE],
                              const struct zmk_key_telemetry_record *record) {
    output[ZMK_KEY_TELEMETRY_OFFSET_VERSION] = ZMK_KEY_TELEMETRY_PROTOCOL_VERSION;
    output[ZMK_KEY_TELEMETRY_OFFSET_TYPE] = (uint8_t)record->type;
    output[ZMK_KEY_TELEMETRY_OFFSET_FLAGS] = record->pressed ? ZMK_KEY_TELEMETRY_FLAG_PRESSED : 0U;
    output[ZMK_KEY_TELEMETRY_OFFSET_POSITION] = record->position;
    put_le16(&output[ZMK_KEY_TELEMETRY_OFFSET_SEQUENCE], record->sequence);
    put_le32(&output[ZMK_KEY_TELEMETRY_OFFSET_TIMESTAMP], record->timestamp_ms);
    put_le32(&output[ZMK_KEY_TELEMETRY_OFFSET_LAYERS], record->active_layers);

    for (size_t i = 0; i < 6U; i++) {
        output[ZMK_KEY_TELEMETRY_OFFSET_POSITIONS + i] =
            (uint8_t)(record->pressed_positions >> (i * 8U));
    }
}
