/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ZMK_KEY_TELEMETRY_PROTOCOL_VERSION 1U
#define ZMK_KEY_TELEMETRY_RECORD_SIZE 20U
#define ZMK_KEY_TELEMETRY_POSITION_COUNT 48U
#define ZMK_KEY_TELEMETRY_POSITION_NONE UINT8_MAX

enum zmk_key_telemetry_message_type {
    ZMK_KEY_TELEMETRY_MSG_SNAPSHOT = 0x01,
    ZMK_KEY_TELEMETRY_MSG_KEY = 0x02,
    ZMK_KEY_TELEMETRY_MSG_LAYERS = 0x03,
};

enum zmk_key_telemetry_record_offset {
    ZMK_KEY_TELEMETRY_OFFSET_VERSION = 0,
    ZMK_KEY_TELEMETRY_OFFSET_TYPE = 1,
    ZMK_KEY_TELEMETRY_OFFSET_FLAGS = 2,
    ZMK_KEY_TELEMETRY_OFFSET_POSITION = 3,
    ZMK_KEY_TELEMETRY_OFFSET_SEQUENCE = 4,
    ZMK_KEY_TELEMETRY_OFFSET_TIMESTAMP = 6,
    ZMK_KEY_TELEMETRY_OFFSET_LAYERS = 10,
    ZMK_KEY_TELEMETRY_OFFSET_POSITIONS = 14,
};

#define ZMK_KEY_TELEMETRY_FLAG_PRESSED (1U << 0)
#define ZMK_KEY_TELEMETRY_KNOWN_FLAGS ZMK_KEY_TELEMETRY_FLAG_PRESSED

struct zmk_key_telemetry_record {
    enum zmk_key_telemetry_message_type type;
    bool pressed;
    uint8_t position;
    uint16_t sequence;
    uint32_t timestamp_ms;
    uint32_t active_layers;
    uint64_t pressed_positions;
};

uint32_t zmk_key_telemetry_complete_layer_mask(uint32_t explicit_layers, uint8_t default_layer);

void zmk_key_telemetry_encode(uint8_t output[ZMK_KEY_TELEMETRY_RECORD_SIZE],
                              const struct zmk_key_telemetry_record *record);
