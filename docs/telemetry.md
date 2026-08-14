# ZMK key telemetry

The reusable external Zephyr module lives at `modules/zmk-key-telemetry` and is
discovered through its own `zephyr/module.yml`. It observes ZMK position and
layer events and exposes one custom GATT service beside normal Bluetooth HID.
Its listeners always return `ZMK_EV_EVENT_BUBBLE`; they do not consume, delay,
rewrite, or synthesize keyboard events.

Telemetry is compiled only into `corne_left`. Normal split processing turns
right-half matrix changes into global position events on the central, so the
central observes both halves without telemetry code or a custom telemetry
message on the peripheral. Positions use the native zero-based `0-35` space of
the 36-entry keymap and `draw/corne.yaml`. Labels, bindings, geometry, and layer
presentation stay in that desktop-side drawing data and are never sent by the
firmware.

## UUIDs and security

- Service: `9e7a7d70-df1b-4f76-9d45-8c3f4a6b2100`
- Record characteristic: `9e7a7d70-df1b-4f76-9d45-8c3f4a6b2101`
- Properties: read and notify
- Permissions: encrypted characteristic reads and encrypted CCC reads/writes

The service UUID is stable but is not advertised. Connect to the bonded BlueZ
device, normally named `Chocochap`, and discover the characteristic. Normal
Just Works keyboard pairing supplies link encryption and bonding but not MITM
authentication, so protocol v1 uses encrypted rather than authenticated GATT
permissions.

## Protocol v1

Every read and notification is exactly 20 bytes and fits the mandatory 23-byte
ATT MTU. Multibyte integers are unsigned little-endian.

| Offset | Width | Field | Meaning |
| ---: | ---: | --- | --- |
| 0 | 1 | `version` | `0x01` |
| 1 | 1 | `type` | `0x01` snapshot, `0x02` key, `0x03` layers |
| 2 | 1 | `flags` | bit 0 is pressed; remaining bits are zero |
| 3 | 1 | `position` | global physical position, or `0xff` |
| 4 | 2 | `sequence` | state revision modulo 65536 |
| 6 | 4 | `timestamp_ms` | central monotonic milliseconds modulo 2^32 |
| 10 | 4 | `active_layers` | complete layer-ID bitmap |
| 14 | 6 | `pressed_positions` | complete 48-position bitmap |

Key records use flag bit zero and position `0-35`. Snapshot and layer records
use zero flags and position `0xff`. The layer mask explicitly includes the
default layer. Protocol v1 retains capacity for 32 layers and 48 positions for
wire compatibility, but this six-layer, 36-key firmware keeps bits 36-47 clear.

Local-left records use the original event timestamp. Right-half records use the
central time when normal split processing reconstructs the position event.
Timestamps and sequence values wrap and must be compared modulo their widths.

### Synchronization and loss

- A read returns a fresh snapshot without advancing the sequence.
- Enabling notifications schedules a snapshot. Reconnection, encryption, and
  an active-profile change also schedule one when the CCC is active.
- Every state revision advances the sequence, even when notifications are
  disabled or cannot be queued.
- Every record repeats complete pressed-position and active-layer state, so a
  client replaces its state from the newest record after a sequence gap.
- A reboot resets sequence and uptime. A reconnect snapshot is a new baseline.
- Invalid lengths and unsupported versions must be rejected, not guessed.

Notifications use bounded, non-blocking work. Queue overflow drops the oldest
pending record; send failures are not retried. Loss remains visible through a
sequence gap and cannot apply backpressure to HID or split processing.

## Build and automated checks

`CONFIG_ZMK_KEY_TELEMETRY` defaults off, depends on BLE and the split-central
role, and is enabled only by `config/corne_left.conf`. Both pristine builds are
given the absolute vendored module path through `ZMK_EXTRA_MODULES`, proving the
right-side Kconfig/CMake exclusion. The module is not a west project and the
pinned ZMK and Zephyr checkouts remain unchanged.

```sh
nix develop
just test
python3 -m py_compile scripts/test-telemetry.py
just build-left
just build-right

# Equivalent direct builds:
west build -p always -s zmk/app -d .build/corne_left-nice_nano_v2 \
  -b nice_nano_v2 -- -DSHIELD=corne_left -DZMK_CONFIG="$PWD/config" \
  -DZMK_EXTRA_MODULES="$PWD/modules/zmk-key-telemetry"
west build -p always -s zmk/app -d .build/corne_right-nice_nano_v2 \
  -b nice_nano_v2 -- -DSHIELD=corne_right -DZMK_CONFIG="$PWD/config" \
  -DZMK_EXTRA_MODULES="$PWD/modules/zmk-key-telemetry"

grep '^CONFIG_ZMK_KEY_TELEMETRY=' .build/*/zephyr/.config
```

The grep should find `CONFIG_ZMK_KEY_TELEMETRY=y` only on the left. The right
image must also contain no telemetry implementation, listener/service
identifiers, or either UUID byte sequence. See `docs/validation.md` for build,
binary-isolation, artifact hash/size, and measured resource evidence.

## Manual BlueZ verification

Retain known-good images. Flash the right/peripheral half first and left/central
half second, then pair or re-pair `Chocochap`. On a host with Bluetooth access:

```sh
python3 scripts/test-telemetry.py
python3 scripts/test-telemetry.py XX:XX:XX:XX:XX:XX
```

The verifier automatically selects one paired `Chocochap` or accepts the
explicit address. It requires Python 3, `bluetoothctl`, an accessible Bluetooth
controller, and the system BlueZ D-Bus service. A NixOS container can perform
syntax, protocol, firmware, and binary-isolation checks, but not live BLE unless
the host controller and system bus are explicitly exposed.

Confirm the initial type-1 snapshot, press/release and overlapping-key type-2
records from both halves, native positions `0-35`, matching pressed bits,
advancing sequences, and normal HID operation. Hold NUM, NAV, FN, and ADJUST
access keys and confirm type-3 records carry the complete six-layer mask.

If the UUID is missing after upgrading an already paired keyboard, remove the
device from BlueZ, clear or select the corresponding ZMK profile, and pair again
to invalidate the stale GATT cache. GATT is exposed through system D-Bus; no
telemetry device node, udev rule, input-device access, or raw-device access is
needed.

Telemetry adds a static service, bounded record queue, and state/work objects
only to the left image. With no subscriber it sends no telemetry radio traffic;
while subscribed, every key edge and layer change adds one 20-byte notification.
Rollback is simply reflashing the retained right image first and left image
second; no stored keymap migration is involved.
