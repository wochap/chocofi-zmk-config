# ZMK key telemetry

The reusable external Zephyr module lives at `modules/zmk-key-telemetry` and is
identified by its own `zephyr/module.yml`. It listens to ZMK's
`zmk_position_state_changed` and `zmk_layer_state_changed` events and exposes one
custom GATT service beside the normal Bluetooth HID service. The listeners
always return `ZMK_EV_EVENT_BUBBLE`; they do not capture, delay, rewrite, or
consume keyboard events.

The service is compiled only into `corne_left`. ZMK's pinned split BLE central
already converts right-half matrix events through the right transform's
`col-offset = <6>` and raises them on the left as global positions. Consequently
positions from either half use the same zero-based 0-41 space as
`config/corne.keymap` and each 42-entry layer in `draw/corne.yaml`.

## UUIDs and access

- Service: `9e7a7d70-df1b-4f76-9d45-8c3f4a6b2100`
- Record characteristic: `9e7a7d70-df1b-4f76-9d45-8c3f4a6b2101`
- Characteristic properties: read and notify
- Characteristic and CCC permissions: encrypted read; encrypted CCC read/write

The service UUID is stable but is not added to the advertising packet. Identify
the keyboard by its bonded BlueZ device (normally named `Chocochap`), connect,
then discover the service UUID. A client should use the BlueZ identity/device
object rather than relying only on the human-readable name.

The access permissions match ZMK's encrypted HID characteristics. Pair the
keyboard normally before reading the characteristic or writing its CCC. With a
Just Works keyboard pairing this provides link encryption and bonding, but not
MITM authentication. Requiring authenticated/MITM GATT access would make the
service unusable with common keyboard pairing flows, so v1 deliberately uses
`BT_GATT_PERM_READ_ENCRYPT` rather than `BT_GATT_PERM_READ_AUTHEN`.

## Protocol v1

Every read response and notification is exactly 20 bytes, so one record fits in
the 20-byte attribute value available at the mandatory 23-byte ATT MTU. All
multibyte integers are unsigned little-endian.

| Offset | Width | Field | Meaning |
| ---: | ---: | --- | --- |
| 0 | 1 | `version` | `0x01` |
| 1 | 1 | `type` | `0x01` snapshot, `0x02` physical key, `0x03` layers |
| 2 | 1 | `flags` | bit 0 is pressed; all other bits are zero |
| 3 | 1 | `position` | global physical position, or `0xff` when not applicable |
| 4 | 2 | `sequence` | state revision modulo 65536 |
| 6 | 4 | `timestamp_ms` | central monotonic uptime in milliseconds, modulo 2^32 |
| 10 | 4 | `active_layers` | complete layer-ID bitmask; bit 0 is layer 0 |
| 14 | 6 | `pressed_positions` | complete 48-position bitmap; bit N is position N |

`flags & 1` is meaningful for key records and matches the resulting bit in
`pressed_positions`. Snapshot and layer records use position `0xff` and flags
zero. The active-layer field includes the default layer even though the pinned
ZMK API stores that layer implicitly. The current keymap has six layers and 42
positions; protocol v1 has capacity for 32 layers and 48 positions.

The timestamp on a local-left event is the ZMK event time. The pinned split
implementation reconstructs a right-half position event when its bitmap arrives
at the central, so a right-half timestamp is central receive time, not a clock
transferred from the peripheral. Timestamps and sequence numbers wrap and must be
compared modulo their field widths.

### Synchronization and loss

- Reading always returns a freshly encoded snapshot and never advances the
  sequence.
- Enabling notifications schedules a snapshot. Reconnection and successful BLE
  security establishment also schedule one when the CCC is active.
- Every record repeats both complete state fields. A client can immediately
  replace its pressed-position and active-layer state with the newest valid
  record, even after a sequence gap.
- A physical or layer change advances the sequence even if notifications are
  disabled or a BLE packet cannot be queued. A modular difference other than
  one indicates one or more missed state revisions. Read once to obtain an
  explicit snapshot if desired; the next record is already authoritative.
- Sequence zero is the boot snapshot until the first state change. On reconnect,
  a snapshot is a new baseline; a firmware reboot resets sequence and uptime.
- If notifications are disabled or unavailable, reads still provide current
  state but there is no event stream. BLE disconnects merely drop telemetry.
  Normal HID processing continues.
- A v1 client must reject any record whose version is not 1 or whose length is
  not 20, unsubscribe, and report an unsupported protocol rather than guessing.

The notification queue is intentionally small and non-blocking. On overflow it
drops its oldest telemetry record. BLE send failures are not retried, avoiding
competition with HID traffic; sequence gaps and repeated authoritative state
make loss visible and recoverable.

## Enabling and building

`CONFIG_ZMK_KEY_TELEMETRY` defaults to `n` and depends on ZMK BLE plus either a
non-split board or the split central role. This keyboard enables it only in
`config/corne_left.conf`. The GitHub build matrix and local Justfile both pass
the vendored module directory through `ZMK_EXTRA_MODULES`; it is not a checkout
or modification of the pinned ZMK source tree.

```sh
nix develop
just test
just build all --pristine

# Equivalent direct builds:
west build -p -s zmk/app -d .build/corne_left-nice_nano_v2 \
  -b nice_nano_v2 -- -DSHIELD=corne_left -DZMK_CONFIG="$PWD/config" \
  -DZMK_EXTRA_MODULES="$PWD/modules/zmk-key-telemetry"
west build -p -s zmk/app -d .build/corne_right-nice_nano_v2 \
  -b nice_nano_v2 -- -DSHIELD=corne_right -DZMK_CONFIG="$PWD/config" \
  -DZMK_EXTRA_MODULES="$PWD/modules/zmk-key-telemetry"

grep '^CONFIG_ZMK_KEY_TELEMETRY=' .build/*/zephyr/.config
```

The expected result is one `CONFIG_ZMK_KEY_TELEMETRY=y` line for the left. The
right has no such line because the Kconfig dependency on the central role is
false. Its image must likewise contain no `key_telemetry` symbols.

## Manual BlueZ verification

Flash both halves (right first, as usual), pair and connect the `Chocochap`
keyboard using the desktop Bluetooth UI or `bluetoothctl`. The small repository
tester finds a single paired device named `Chocochap`, subscribes, and decodes
the records:

```sh
python3 scripts/test-telemetry.py

# If auto-detection is ambiguous, pass the BlueZ device address:
python3 scripts/test-telemetry.py XX:XX:XX:XX:XX:XX
```

It requires only Python 3 and `bluetoothctl`, does not need root, and does not
open any input device. Alternatively, use `bluetoothctl`'s GATT menu directly:

```text
bluetoothctl
scan on
pair XX:XX:XX:XX:XX:XX
trust XX:XX:XX:XX:XX:XX
connect XX:XX:XX:XX:XX:XX
list-attributes XX:XX:XX:XX:XX:XX
menu gatt
select-attribute 9e7a7d70-df1b-4f76-9d45-8c3f4a6b2101
read
notify on
```

The initial read/notification should be a 20-byte type-1 record. Press keys on
both halves and look for type 2, the global position at byte 3, flag bit 0, and
the matching bit at bytes 14-19. Hold the NUM/NAV/FN/ADJUST layer keys and look
for type 3 records and the full mask at bytes 10-13. Multiple bits may be set;
the desktop client may choose the highest effective layer using the layer order
in `draw/corne.yaml`.

If an already bonded host does not discover the newly added UUID after flashing,
remove the keyboard from BlueZ, clear/select the matching ZMK profile, and pair
again so BlueZ cannot reuse an older cached GATT database.

BlueZ exposes GATT over its system D-Bus API. No telemetry character device
appears under `/dev`, and no udev rule is required. The normal HID stack may
create `/dev/input/event*`, but a telemetry client must not open it. The minimum
device-node permission requirement is therefore none: do not grant `input`,
`uinput`, serial, or raw-device access. A packaged desktop service may need the
normal system D-Bus/BlueZ policy (or a narrowly scoped D-Bus policy), which is
not a udev permission.

## Resource and transport effects

A controlled build against the same config with only
`CONFIG_ZMK_KEY_TELEMETRY` disabled measured the feature at 1,648 bytes of flash
and 392 bytes of RAM on `corne_left` (221,272/54,072 bytes enabled versus
219,624/53,680 bytes disabled). That includes one static GATT service, a
160-byte record payload queue at the default queue depth, and state/work objects.
The right firmware contains no telemetry code or UUID. When nobody subscribes,
events only update a small in-memory bitmap and sequence; there is no telemetry
radio traffic. While subscribed, each physical key edge and layer change adds
one 20-byte GATT notification, so power use is higher than plain HID use. The
service does not alter USB descriptors, does not require USB, and works during
Bluetooth-only operation alongside the normal HID service.

## Desktop-client next steps

Use a BlueZ D-Bus library to locate the bonded device and UUID, pair/connect,
read one snapshot, and subscribe to `PropertiesChanged` for characteristic
values. Validate length/version/type/reserved bits, compare sequence modulo
65536, and replace local state from every record. Correlate set position bits
with the same indices in `draw/corne.yaml`; keep all label/keycode and effective
layer selection logic on the desktop.
