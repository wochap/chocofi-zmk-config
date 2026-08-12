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

#include <zmk/ble.h>
#include <zmk/event_manager.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/matrix.h>

#include <zmk_key_telemetry/protocol.h>

LOG_MODULE_REGISTER(zmk_key_telemetry, CONFIG_ZMK_LOG_LEVEL);

/*
 * 9e7a7d70-df1b-4f76-9d45-8c3f4a6b2100 service
 * 9e7a7d70-df1b-4f76-9d45-8c3f4a6b2101 record characteristic
 */
#define ZMK_KEY_TELEMETRY_SERVICE_UUID                                                             \
    BT_UUID_128_ENCODE(0x9e7a7d70, 0xdf1b, 0x4f76, 0x9d45, 0x8c3f4a6b2100)
#define ZMK_KEY_TELEMETRY_RECORD_UUID                                                              \
    BT_UUID_128_ENCODE(0x9e7a7d70, 0xdf1b, 0x4f76, 0x9d45, 0x8c3f4a6b2101)

#define TELEMETRY_VALUE_ATTR_INDEX 1

BUILD_ASSERT(ZMK_KEYMAP_LEN <= ZMK_KEY_TELEMETRY_POSITION_COUNT,
             "key telemetry v1 supports at most 48 key positions");
BUILD_ASSERT(ZMK_KEYMAP_LAYERS_LEN <= 32, "key telemetry v1 supports at most 32 layers");
BUILD_ASSERT(ZMK_KEY_TELEMETRY_RECORD_SIZE <= (23U - 3U),
             "key telemetry record must fit the default ATT MTU");

static struct k_spinlock telemetry_lock;
static uint64_t pressed_positions;
static uint16_t sequence;
static atomic_t notifications_enabled;

K_MSGQ_DEFINE(notification_queue, ZMK_KEY_TELEMETRY_RECORD_SIZE,
              CONFIG_ZMK_KEY_TELEMETRY_QUEUE_SIZE, 4);

static uint32_t complete_layer_mask(void) {
    return zmk_key_telemetry_complete_layer_mask(zmk_keymap_layer_state(),
                                                 zmk_keymap_layer_default());
}

static void encode_snapshot_locked(uint8_t output[ZMK_KEY_TELEMETRY_RECORD_SIZE]) {
    const struct zmk_key_telemetry_record snapshot = {
        .type = ZMK_KEY_TELEMETRY_MSG_SNAPSHOT,
        .pressed = false,
        .position = ZMK_KEY_TELEMETRY_POSITION_NONE,
        .sequence = sequence,
        .timestamp_ms = (uint32_t)k_uptime_get(),
        .active_layers = complete_layer_mask(),
        .pressed_positions = pressed_positions,
    };
    zmk_key_telemetry_encode(output, &snapshot);
}

static void encode_snapshot(uint8_t output[ZMK_KEY_TELEMETRY_RECORD_SIZE]) {
    k_spinlock_key_t key = k_spin_lock(&telemetry_lock);
    encode_snapshot_locked(output);
    k_spin_unlock(&telemetry_lock, key);
}

static ssize_t read_record(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
                           uint16_t len, uint16_t offset) {
    uint8_t snapshot[ZMK_KEY_TELEMETRY_RECORD_SIZE];
    encode_snapshot(snapshot);
    return bt_gatt_attr_read(conn, attr, buf, len, offset, snapshot, sizeof(snapshot));
}

static void notification_work_handler(struct k_work *work);
static void sync_work_handler(struct k_work *work);
K_WORK_DEFINE(notification_work, notification_work_handler);
K_WORK_DEFINE(sync_work, sync_work_handler);

static void ccc_changed(const struct bt_gatt_attr *attr, uint16_t value) {
    ARG_UNUSED(attr);

    if (value != BT_GATT_CCC_NOTIFY) {
        atomic_clear(&notifications_enabled);
        k_msgq_purge(&notification_queue);
        return;
    }

    k_work_submit(&sync_work);
}

BT_GATT_SERVICE_DEFINE(key_telemetry_service,
                       BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_128(ZMK_KEY_TELEMETRY_SERVICE_UUID)),
                       BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(ZMK_KEY_TELEMETRY_RECORD_UUID),
                                              BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                                              BT_GATT_PERM_READ_ENCRYPT, read_record, NULL, NULL),
                       BT_GATT_CCC(ccc_changed,
                                   BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT));

static bool connection_can_notify(struct bt_conn *conn) {
    return conn && bt_gatt_get_mtu(conn) >= (ZMK_KEY_TELEMETRY_RECORD_SIZE + 3U) &&
           bt_gatt_is_subscribed(conn, &key_telemetry_service.attrs[TELEMETRY_VALUE_ATTR_INDEX],
                                 BT_GATT_CCC_NOTIFY);
}

/* Caller holds telemetry_lock so sequence order and queue order stay identical. */
static bool queue_record_locked(const uint8_t record[ZMK_KEY_TELEMETRY_RECORD_SIZE]) {
    if (!atomic_get(&notifications_enabled)) {
        return false;
    }

    if (k_msgq_put(&notification_queue, record, K_NO_WAIT) != 0) {
        uint8_t discarded[ZMK_KEY_TELEMETRY_RECORD_SIZE];
        (void)k_msgq_get(&notification_queue, discarded, K_NO_WAIT);
        if (k_msgq_put(&notification_queue, record, K_NO_WAIT) != 0) {
            return false;
        }
    }

    return true;
}

static void notification_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    struct bt_conn *conn = zmk_ble_active_profile_conn();
    if (!connection_can_notify(conn)) {
        atomic_clear(&notifications_enabled);
        k_msgq_purge(&notification_queue);
        if (conn) {
            bt_conn_unref(conn);
        }
        return;
    }

    uint8_t record[ZMK_KEY_TELEMETRY_RECORD_SIZE];
    while (k_msgq_get(&notification_queue, record, K_NO_WAIT) == 0) {
        int err = bt_gatt_notify(conn, &key_telemetry_service.attrs[TELEMETRY_VALUE_ATTR_INDEX],
                                 record, sizeof(record));
        if (err) {
            LOG_DBG("Telemetry notification dropped (%d)", err);
        }
    }

    bt_conn_unref(conn);
}

static void sync_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    struct bt_conn *conn = zmk_ble_active_profile_conn();
    if (!connection_can_notify(conn)) {
        atomic_clear(&notifications_enabled);
        k_msgq_purge(&notification_queue);
        if (conn) {
            bt_conn_unref(conn);
        }
        return;
    }

    uint8_t snapshot[ZMK_KEY_TELEMETRY_RECORD_SIZE];
    k_spinlock_key_t key = k_spin_lock(&telemetry_lock);
    atomic_clear(&notifications_enabled);
    k_msgq_purge(&notification_queue);
    encode_snapshot_locked(snapshot);
    atomic_set(&notifications_enabled, true);
    bool queued = queue_record_locked(snapshot);
    k_spin_unlock(&telemetry_lock, key);

    if (queued) {
        k_work_submit(&notification_work);
    }
    bt_conn_unref(conn);
}

static int telemetry_event_listener(const zmk_event_t *event) {
    uint8_t encoded[ZMK_KEY_TELEMETRY_RECORD_SIZE];
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
        sequence++;
        const struct zmk_key_telemetry_record record = {
            .type = ZMK_KEY_TELEMETRY_MSG_KEY,
            .pressed = position_event->state,
            .position = (uint8_t)position_event->position,
            .sequence = sequence,
            .timestamp_ms = (uint32_t)position_event->timestamp,
            .active_layers = complete_layer_mask(),
            .pressed_positions = pressed_positions,
        };
        zmk_key_telemetry_encode(encoded, &record);
        bool queued = queue_record_locked(encoded);
        k_spin_unlock(&telemetry_lock, key);
        if (queued) {
            k_work_submit(&notification_work);
        }
        return ZMK_EV_EVENT_BUBBLE;
    }

    const struct zmk_layer_state_changed *layer_event = as_zmk_layer_state_changed(event);
    if (layer_event) {
        k_spinlock_key_t key = k_spin_lock(&telemetry_lock);
        sequence++;
        const struct zmk_key_telemetry_record record = {
            .type = ZMK_KEY_TELEMETRY_MSG_LAYERS,
            .pressed = false,
            .position = ZMK_KEY_TELEMETRY_POSITION_NONE,
            .sequence = sequence,
            .timestamp_ms = (uint32_t)layer_event->timestamp,
            .active_layers = complete_layer_mask(),
            .pressed_positions = pressed_positions,
        };
        zmk_key_telemetry_encode(encoded, &record);
        bool queued = queue_record_locked(encoded);
        k_spin_unlock(&telemetry_lock, key);
        if (queued) {
            k_work_submit(&notification_work);
        }
    }

    return ZMK_EV_EVENT_BUBBLE;
}

static int profile_event_listener(const zmk_event_t *event) {
    ARG_UNUSED(event);
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

ZMK_LISTENER(key_telemetry, telemetry_event_listener);
ZMK_SUBSCRIPTION(key_telemetry, zmk_position_state_changed);
ZMK_SUBSCRIPTION(key_telemetry, zmk_layer_state_changed);

ZMK_LISTENER(key_telemetry_profile, profile_event_listener);
ZMK_SUBSCRIPTION(key_telemetry_profile, zmk_ble_active_profile_changed);
