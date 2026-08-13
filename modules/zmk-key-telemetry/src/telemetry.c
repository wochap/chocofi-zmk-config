/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/hid_indicators_changed.h>
#include <zmk/events/hid_modifiers_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/hid.h>
#include <zmk/hid_indicators.h>
#include <zmk/keymap.h>
#include <zmk/matrix.h>

#include <zmk_key_telemetry/protocol.h>

LOG_MODULE_REGISTER(zmk_key_telemetry, CONFIG_ZMK_LOG_LEVEL);

/*
 * 9e7a7d70-df1b-4f76-9d45-8c3f4a6b2100 service
 * 9e7a7d70-df1b-4f76-9d45-8c3f4a6b2101 state-frame characteristic
 */
#define ZMK_KEY_TELEMETRY_SERVICE_UUID                                                             \
    BT_UUID_128_ENCODE(0x9e7a7d70, 0xdf1b, 0x4f76, 0x9d45, 0x8c3f4a6b2100)
#define ZMK_KEY_TELEMETRY_FRAME_UUID                                                              \
    BT_UUID_128_ENCODE(0x9e7a7d70, 0xdf1b, 0x4f76, 0x9d45, 0x8c3f4a6b2101)

#define TELEMETRY_VALUE_ATTR_INDEX 1
#define TELEMETRY_COALESCE_DELAY K_MSEC(CONFIG_ZMK_KEY_TELEMETRY_COALESCE_MS)
#define TELEMETRY_MIN_ATT_MTU (ZMK_KEY_TELEMETRY_FRAME_SIZE + 3U)

BUILD_ASSERT(ZMK_KEYMAP_LEN <= ZMK_KEY_TELEMETRY_POSITION_COUNT,
             "key telemetry v2 supports at most 64 key positions");
BUILD_ASSERT(ZMK_KEYMAP_LAYERS_LEN <= 32, "key telemetry v2 supports at most 32 layers");
BUILD_ASSERT(ZMK_KEY_TELEMETRY_FRAME_SIZE == 48U, "key telemetry v2 frame layout changed");
BUILD_ASSERT(ZMK_KEY_TELEMETRY_FRAME_SIZE + 3U <= CONFIG_BT_L2CAP_TX_MTU,
             "key telemetry v2 frame exceeds the local ATT/L2CAP transmit MTU");

static struct k_spinlock telemetry_lock;
static uint64_t pressed_positions;
static uint32_t sequence;
static uint32_t dropped_frames;
static uint8_t peripheral_battery_pct = ZMK_KEY_TELEMETRY_VALUE_UNKNOWN;
static bool peripheral_battery_valid;
static atomic_t dirty_fields;
static atomic_t notifications_enabled;

static void flush_work_handler(struct k_work *work);
static void sync_work_handler(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(flush_work, flush_work_handler);
K_WORK_DEFINE(sync_work, sync_work_handler);

static uint32_t complete_layer_mask(uint8_t default_layer) {
    return zmk_key_telemetry_complete_layer_mask(zmk_keymap_layer_state(), default_layer);
}

static void mark_dirty(uint32_t fields) {
    atomic_or(&dirty_fields, (atomic_val_t)fields);
    (void)k_work_reschedule(&flush_work, TELEMETRY_COALESCE_DELAY);
}

static void capture_frame(struct zmk_key_telemetry_frame *frame, bool snapshot,
                          uint32_t changed_fields) {
    const uint8_t default_layer = zmk_keymap_layer_default();
    const struct zmk_endpoint_instance endpoint = zmk_endpoints_selected();

    *frame = (struct zmk_key_telemetry_frame){
        .flags = snapshot ? ZMK_KEY_TELEMETRY_FLAG_SNAPSHOT : 0U,
        .timestamp_ms = (uint64_t)k_uptime_get(),
        .active_layers = complete_layer_mask(default_layer),
        .changed_fields = snapshot ? 0U : changed_fields,
        .valid_fields = ZMK_KEY_TELEMETRY_FIELD_POSITIONS | ZMK_KEY_TELEMETRY_FIELD_LAYERS |
                        ZMK_KEY_TELEMETRY_FIELD_MODIFIERS |
                        ZMK_KEY_TELEMETRY_FIELD_DEFAULT_LAYER | ZMK_KEY_TELEMETRY_FIELD_ENDPOINT,
        .modifiers = zmk_hid_get_keyboard_report()->body.modifiers,
        .default_layer = default_layer,
        .transport = ZMK_KEY_TELEMETRY_TRANSPORT_UNKNOWN,
        .ble_profile = ZMK_KEY_TELEMETRY_VALUE_UNKNOWN,
        .central_battery_pct = ZMK_KEY_TELEMETRY_VALUE_UNKNOWN,
        .peripheral_battery_pct = ZMK_KEY_TELEMETRY_VALUE_UNKNOWN,
        .split_status = ZMK_KEY_TELEMETRY_SPLIT_UNKNOWN,
    };

    switch (endpoint.transport) {
    case ZMK_TRANSPORT_USB:
        frame->transport = ZMK_KEY_TELEMETRY_TRANSPORT_USB;
        break;
    case ZMK_TRANSPORT_BLE:
        frame->transport = ZMK_KEY_TELEMETRY_TRANSPORT_BLE;
        frame->ble_profile = (uint8_t)endpoint.ble.profile_index;
        break;
    default:
        frame->valid_fields &= ~ZMK_KEY_TELEMETRY_FIELD_ENDPOINT;
        break;
    }

#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
    frame->hid_indicators = zmk_hid_indicators_get_current_profile();
    frame->valid_fields |= ZMK_KEY_TELEMETRY_FIELD_HID_INDICATORS;
#endif

#if IS_ENABLED(CONFIG_ZMK_BATTERY)
    frame->central_battery_pct = zmk_battery_state_of_charge();
    frame->valid_fields |= ZMK_KEY_TELEMETRY_FIELD_CENTRAL_BATTERY;
#endif

    k_spinlock_key_t key = k_spin_lock(&telemetry_lock);
    frame->pressed_positions = pressed_positions;
    frame->sequence = sequence;
    frame->dropped_frames = dropped_frames;
    if (peripheral_battery_valid) {
        frame->peripheral_battery_pct = peripheral_battery_pct;
        frame->valid_fields |= ZMK_KEY_TELEMETRY_FIELD_PERIPHERAL_BATTERY;
    }
    k_spin_unlock(&telemetry_lock, key);
}

static void encode_snapshot(uint8_t output[ZMK_KEY_TELEMETRY_FRAME_SIZE]) {
    struct zmk_key_telemetry_frame frame;
    capture_frame(&frame, true, 0U);
    zmk_key_telemetry_encode(output, &frame);
}

static ssize_t read_frame(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
                          uint16_t len, uint16_t offset) {
    uint8_t snapshot[ZMK_KEY_TELEMETRY_FRAME_SIZE];
    encode_snapshot(snapshot);
    return bt_gatt_attr_read(conn, attr, buf, len, offset, snapshot, sizeof(snapshot));
}

static void ccc_changed(const struct bt_gatt_attr *attr, uint16_t value) {
    ARG_UNUSED(attr);

    if (value != BT_GATT_CCC_NOTIFY) {
        atomic_clear(&notifications_enabled);
        return;
    }

    k_work_submit(&sync_work);
}

BT_GATT_SERVICE_DEFINE(key_telemetry_service,
                       BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_128(ZMK_KEY_TELEMETRY_SERVICE_UUID)),
                       BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(ZMK_KEY_TELEMETRY_FRAME_UUID),
                                              BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                                              BT_GATT_PERM_READ_ENCRYPT, read_frame, NULL, NULL),
                       BT_GATT_CCC(ccc_changed,
                                   BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT));

static bool connection_is_subscribed(struct bt_conn *conn) {
    return conn &&
           bt_gatt_is_subscribed(conn, &key_telemetry_service.attrs[TELEMETRY_VALUE_ATTR_INDEX],
                                 BT_GATT_CCC_NOTIFY);
}

static void record_drop(void) {
    k_spinlock_key_t key = k_spin_lock(&telemetry_lock);
    dropped_frames++;
    k_spin_unlock(&telemetry_lock, key);
}

static bool notify_encoded(const uint8_t encoded[ZMK_KEY_TELEMETRY_FRAME_SIZE]) {
    if (!atomic_get(&notifications_enabled)) {
        return false;
    }

    struct bt_conn *conn = zmk_ble_active_profile_conn();
    if (!connection_is_subscribed(conn)) {
        record_drop();
        if (conn) {
            bt_conn_unref(conn);
        }
        return false;
    }

    const uint16_t mtu = bt_gatt_get_mtu(conn);
    if (mtu < TELEMETRY_MIN_ATT_MTU) {
        LOG_WRN("Telemetry v2 needs ATT MTU %u, negotiated %u", TELEMETRY_MIN_ATT_MTU, mtu);
        record_drop();
        bt_conn_unref(conn);
        return false;
    }

    const int err = bt_gatt_notify(conn, &key_telemetry_service.attrs[TELEMETRY_VALUE_ATTR_INDEX],
                                   encoded, ZMK_KEY_TELEMETRY_FRAME_SIZE);
    bt_conn_unref(conn);
    if (err) {
        LOG_DBG("Telemetry notification dropped (%d)", err);
        record_drop();
        return false;
    }

    return true;
}

static void flush_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    const uint32_t changed_fields = (uint32_t)atomic_set(&dirty_fields, 0);
    if (changed_fields == 0U) {
        return;
    }

    k_spinlock_key_t key = k_spin_lock(&telemetry_lock);
    sequence++;
    k_spin_unlock(&telemetry_lock, key);

    struct zmk_key_telemetry_frame frame;
    uint8_t encoded[ZMK_KEY_TELEMETRY_FRAME_SIZE];
    capture_frame(&frame, false, changed_fields);
    zmk_key_telemetry_encode(encoded, &frame);
    (void)notify_encoded(encoded);
}

static void sync_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    struct bt_conn *conn = zmk_ble_active_profile_conn();
    if (!connection_is_subscribed(conn)) {
        atomic_clear(&notifications_enabled);
        if (conn) {
            bt_conn_unref(conn);
        }
        return;
    }
    bt_conn_unref(conn);

    atomic_set(&notifications_enabled, true);
    uint8_t snapshot[ZMK_KEY_TELEMETRY_FRAME_SIZE];
    encode_snapshot(snapshot);
    (void)notify_encoded(snapshot);
}

static int telemetry_state_listener(const zmk_event_t *event) {
    const struct zmk_position_state_changed *position_event = as_zmk_position_state_changed(event);
    if (position_event) {
        if (position_event->position >= ZMK_KEY_TELEMETRY_POSITION_COUNT) {
            return ZMK_EV_EVENT_BUBBLE;
        }

        k_spinlock_key_t key = k_spin_lock(&telemetry_lock);
        const uint64_t position_bit = UINT64_C(1) << position_event->position;
        if (position_event->state) {
            pressed_positions |= position_bit;
        } else {
            pressed_positions &= ~position_bit;
        }
        k_spin_unlock(&telemetry_lock, key);
        mark_dirty(ZMK_KEY_TELEMETRY_FIELD_POSITIONS);
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (as_zmk_layer_state_changed(event)) {
        mark_dirty(ZMK_KEY_TELEMETRY_FIELD_LAYERS | ZMK_KEY_TELEMETRY_FIELD_DEFAULT_LAYER);
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (as_zmk_hid_modifiers_changed(event)) {
        mark_dirty(ZMK_KEY_TELEMETRY_FIELD_MODIFIERS);
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (as_zmk_endpoint_changed(event)) {
        mark_dirty(ZMK_KEY_TELEMETRY_FIELD_ENDPOINT | ZMK_KEY_TELEMETRY_FIELD_HID_INDICATORS);
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (as_zmk_hid_indicators_changed(event)) {
        mark_dirty(ZMK_KEY_TELEMETRY_FIELD_HID_INDICATORS);
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (as_zmk_battery_state_changed(event)) {
        mark_dirty(ZMK_KEY_TELEMETRY_FIELD_CENTRAL_BATTERY);
        return ZMK_EV_EVENT_BUBBLE;
    }

    const struct zmk_peripheral_battery_state_changed *peripheral_battery_event =
        as_zmk_peripheral_battery_state_changed(event);
    if (peripheral_battery_event && peripheral_battery_event->source == 0U) {
        k_spinlock_key_t key = k_spin_lock(&telemetry_lock);
        peripheral_battery_pct = peripheral_battery_event->state_of_charge;
        peripheral_battery_valid = true;
        k_spin_unlock(&telemetry_lock, key);
        mark_dirty(ZMK_KEY_TELEMETRY_FIELD_PERIPHERAL_BATTERY);
        return ZMK_EV_EVENT_BUBBLE;
    }

    return ZMK_EV_EVENT_BUBBLE;
}

static int profile_event_listener(const zmk_event_t *event) {
    ARG_UNUSED(event);
    mark_dirty(ZMK_KEY_TELEMETRY_FIELD_ENDPOINT | ZMK_KEY_TELEMETRY_FIELD_HID_INDICATORS);
    k_work_submit(&sync_work);
    return ZMK_EV_EVENT_BUBBLE;
}

static void telemetry_security_changed(struct bt_conn *conn, bt_security_t level,
                                       enum bt_security_err err) {
    struct bt_conn_info info;
    if (!err && level >= BT_SECURITY_L2 && bt_conn_get_info(conn, &info) == 0 &&
        info.role == BT_CONN_ROLE_PERIPHERAL) {
        k_work_submit(&sync_work);
    }
}

BT_CONN_CB_DEFINE(key_telemetry_conn_callbacks) = {
    .security_changed = telemetry_security_changed,
};

ZMK_LISTENER(key_telemetry, telemetry_state_listener);
ZMK_SUBSCRIPTION(key_telemetry, zmk_position_state_changed);
ZMK_SUBSCRIPTION(key_telemetry, zmk_layer_state_changed);
ZMK_SUBSCRIPTION(key_telemetry, zmk_hid_modifiers_changed);
ZMK_SUBSCRIPTION(key_telemetry, zmk_endpoint_changed);
ZMK_SUBSCRIPTION(key_telemetry, zmk_hid_indicators_changed);
ZMK_SUBSCRIPTION(key_telemetry, zmk_battery_state_changed);
ZMK_SUBSCRIPTION(key_telemetry, zmk_peripheral_battery_state_changed);

ZMK_LISTENER(key_telemetry_profile, profile_event_listener);
ZMK_SUBSCRIPTION(key_telemetry_profile, zmk_ble_active_profile_changed);
