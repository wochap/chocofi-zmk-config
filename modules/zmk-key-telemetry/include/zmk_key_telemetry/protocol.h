/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#define ZMK_KEY_TELEMETRY_PROTOCOL_VERSION 2U
#define ZMK_KEY_TELEMETRY_FRAME_SIZE 48U
#define ZMK_KEY_TELEMETRY_POSITION_COUNT 64U
#define ZMK_KEY_TELEMETRY_VALUE_UNKNOWN UINT8_MAX

#define ZMK_KEY_TELEMETRY_FLAG_SNAPSHOT (UINT8_C(1) << 0)
#define ZMK_KEY_TELEMETRY_KNOWN_FLAGS ZMK_KEY_TELEMETRY_FLAG_SNAPSHOT

#define ZMK_KEY_TELEMETRY_FIELD_POSITIONS (UINT32_C(1) << 0)
#define ZMK_KEY_TELEMETRY_FIELD_LAYERS (UINT32_C(1) << 1)
#define ZMK_KEY_TELEMETRY_FIELD_MODIFIERS (UINT32_C(1) << 2)
#define ZMK_KEY_TELEMETRY_FIELD_HID_INDICATORS (UINT32_C(1) << 3)
#define ZMK_KEY_TELEMETRY_FIELD_DEFAULT_LAYER (UINT32_C(1) << 4)
#define ZMK_KEY_TELEMETRY_FIELD_ENDPOINT (UINT32_C(1) << 5)
#define ZMK_KEY_TELEMETRY_FIELD_CENTRAL_BATTERY (UINT32_C(1) << 6)
#define ZMK_KEY_TELEMETRY_FIELD_PERIPHERAL_BATTERY (UINT32_C(1) << 7)
#define ZMK_KEY_TELEMETRY_FIELD_SPLIT_STATUS (UINT32_C(1) << 8)
#define ZMK_KEY_TELEMETRY_KNOWN_FIELDS (UINT32_C(0x000001ff))

enum zmk_key_telemetry_transport {
    ZMK_KEY_TELEMETRY_TRANSPORT_UNKNOWN = 0,
    ZMK_KEY_TELEMETRY_TRANSPORT_USB = 1,
    ZMK_KEY_TELEMETRY_TRANSPORT_BLE = 2,
};

enum zmk_key_telemetry_split_status {
    ZMK_KEY_TELEMETRY_SPLIT_UNKNOWN = 0,
    ZMK_KEY_TELEMETRY_SPLIT_DISCONNECTED = 1,
    ZMK_KEY_TELEMETRY_SPLIT_CONNECTED = 2,
};

enum zmk_key_telemetry_frame_offset {
    ZMK_KEY_TELEMETRY_OFFSET_VERSION = 0,
    ZMK_KEY_TELEMETRY_OFFSET_FLAGS = 1,
    ZMK_KEY_TELEMETRY_OFFSET_FRAME_SIZE = 2,
    ZMK_KEY_TELEMETRY_OFFSET_SEQUENCE = 4,
    ZMK_KEY_TELEMETRY_OFFSET_TIMESTAMP = 8,
    ZMK_KEY_TELEMETRY_OFFSET_POSITIONS = 16,
    ZMK_KEY_TELEMETRY_OFFSET_LAYERS = 24,
    ZMK_KEY_TELEMETRY_OFFSET_CHANGED_FIELDS = 28,
    ZMK_KEY_TELEMETRY_OFFSET_VALID_FIELDS = 32,
    ZMK_KEY_TELEMETRY_OFFSET_MODIFIERS = 36,
    ZMK_KEY_TELEMETRY_OFFSET_HID_INDICATORS = 37,
    ZMK_KEY_TELEMETRY_OFFSET_DEFAULT_LAYER = 38,
    ZMK_KEY_TELEMETRY_OFFSET_TRANSPORT = 39,
    ZMK_KEY_TELEMETRY_OFFSET_BLE_PROFILE = 40,
    ZMK_KEY_TELEMETRY_OFFSET_CENTRAL_BATTERY = 41,
    ZMK_KEY_TELEMETRY_OFFSET_PERIPHERAL_BATTERY = 42,
    ZMK_KEY_TELEMETRY_OFFSET_SPLIT_STATUS = 43,
    ZMK_KEY_TELEMETRY_OFFSET_DROPPED_FRAMES = 44,
};

struct zmk_key_telemetry_frame {
    uint8_t flags;
    uint32_t sequence;
    uint64_t timestamp_ms;
    uint64_t pressed_positions;
    uint32_t active_layers;
    uint32_t changed_fields;
    uint32_t valid_fields;
    uint8_t modifiers;
    uint8_t hid_indicators;
    uint8_t default_layer;
    uint8_t transport;
    uint8_t ble_profile;
    uint8_t central_battery_pct;
    uint8_t peripheral_battery_pct;
    uint8_t split_status;
    uint32_t dropped_frames;
};

uint32_t zmk_key_telemetry_complete_layer_mask(uint32_t explicit_layers, uint8_t default_layer);

void zmk_key_telemetry_encode(uint8_t output[ZMK_KEY_TELEMETRY_FRAME_SIZE],
                              const struct zmk_key_telemetry_frame *frame);
