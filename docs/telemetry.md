# ZMK key telemetry

The reusable external Zephyr module lives at `modules/zmk-key-telemetry` and is
discovered through its own `zephyr/module.yml`. It exposes one custom GATT
service beside normal Bluetooth HID and is compiled only into `corne_left`.
Normal split processing turns right-half matrix changes into global position
events on the central, so the peripheral needs no telemetry implementation.

Listeners always return `ZMK_EV_EVENT_BUBBLE`. Position, layer, modifier,
endpoint, indicator, and battery events mark typed fields dirty and schedule a
1 ms delayable work item. The work item runs after the synchronous ZMK event
chain has settled, samples authoritative state, and emits at most one
coalesced state frame. It never consumes, rewrites, or synthesizes keyboard
events and cannot apply backpressure to HID or split processing.

Positions use the native zero-based `0-35` space of the 36-entry keymap and
`draw/corne.yaml`. Labels, bindings, geometry, shifted glyphs, and layer
presentation stay in desktop data and are never sent by firmware.

## UUIDs and security

- Service: `9e7a7d70-df1b-4f76-9d45-8c3f4a6b2100`
- State-frame characteristic: `9e7a7d70-df1b-4f76-9d45-8c3f4a6b2101`
- Properties: read and notify
- Permissions: encrypted characteristic reads and encrypted CCC reads/writes

The service UUID is stable but is not advertised. Connect to the bonded BlueZ
device, normally named `Chocochap`, and discover the characteristic. Normal
Just Works keyboard pairing supplies link encryption and bonding but not MITM
authentication, so the characteristic uses encrypted rather than authenticated
GATT permissions.

## Protocol v2

Every read and notification is exactly 48 bytes. Multibyte integers are
unsigned little-endian. Protocol v2 is intentionally incompatible with the old
20-byte record protocol; firmware and consumers must be deployed together.

| Offset | Width | Field | Meaning |
| ---: | ---: | --- | --- |
| 0 | 1 | `version` | `0x02` |
| 1 | 1 | `flags` | bit 0 marks a read/reconnect snapshot baseline |
| 2 | 2 | `frame_size` | `48`, permits strict framing checks |
| 4 | 4 | `sequence` | state revision modulo 2^32 |
| 8 | 8 | `timestamp_ms` | central monotonic milliseconds modulo 2^64 |
| 16 | 8 | `pressed_positions` | complete 64-position bitmap |
| 24 | 4 | `active_layers` | complete 32-layer bitmap, including default |
| 28 | 4 | `changed_fields` | fields dirtied since the previous revision |
| 32 | 4 | `valid_fields` | fields whose values are currently authoritative |
| 36 | 1 | `modifiers` | effective HID modifier byte |
| 37 | 1 | `hid_indicators` | host LED output bits from ZMK |
| 38 | 1 | `default_layer` | current default layer ID |
| 39 | 1 | `transport` | 0 unknown, 1 USB, 2 BLE |
| 40 | 1 | `ble_profile` | selected BLE profile, or `0xff` |
| 41 | 1 | `central_battery_pct` | 0-100, or `0xff` |
| 42 | 1 | `peripheral_battery_pct` | 0-100, or `0xff` |
| 43 | 1 | `split_status` | 0 unknown, 1 disconnected, 2 connected |
| 44 | 4 | `dropped_frames` | cumulative failed notification attempts |

`changed_fields` and `valid_fields` use the same bit assignments:

| Bit | Mask | Field group |
| ---: | ---: | --- |
| 0 | `0x00000001` | pressed positions |
| 1 | `0x00000002` | active layers |
| 2 | `0x00000004` | effective modifiers |
| 3 | `0x00000008` | HID indicators |
| 4 | `0x00000010` | default layer |
| 5 | `0x00000020` | transport and BLE profile |
| 6 | `0x00000040` | central battery |
| 7 | `0x00000080` | peripheral battery |
| 8 | `0x00000100` | split status |

Consumers must reject invalid length, version, declared size, flags, enum, and
valid-value combinations. Unknown future field-mask bits may be retained and
ignored. A field with its validity bit clear must not be presented as current,
even if its byte contains a sentinel or stale value.

The HID modifier byte follows the standard bit order: LCTL, LSFT, LALT, LGUI,
RCTL, RSFT, RALT, RGUI. Its source is the forked ZMK
`zmk_hid_modifiers_changed` snapshot event. That event covers explicit,
implicit, masked, and report-clearing changes and fires synchronously only
after `keyboard_report.body.modifiers` contains the new effective byte. The
deferred telemetry work samples that report byte rather than reconstructing
modifier semantics from key bindings.

HID indicators and central split-battery fetching are enabled on the central.
Endpoint, local battery, default layer, modifiers, positions, and layers can be
sampled immediately. Peripheral battery remains invalid until its first
central-side ZMK event, avoiding invented startup state. Split status remains
invalid in this pinned version: ZMK's public split-status event is emitted by
the peripheral implementation, not by the central where telemetry runs. A
central connection hook solely for this optional field would be unnecessarily
invasive.

### Synchronization, coalescing, and loss

- A characteristic read returns a fresh snapshot with the current sequence and
  does not advance it.
- Enabling notifications, reconnection, encryption, or an active-profile
  change schedules a snapshot while CCC is active.
- Each settled dirty batch advances the 32-bit sequence once, even if no client
  is subscribed.
- Every frame repeats the complete authoritative state, so consumers replace
  state rather than applying event deltas.
- A non-snapshot sequence jump reports transport/coalescing loss. A snapshot
  establishes a new gap-detection baseline.
- `dropped_frames` exposes cumulative notification attempts that could not be
  sent; it is reported in the next successful frame.
- A reboot resets sequence, uptime, and dropped-frame count. A reconnect
  snapshot is a new baseline.

There is no firmware record queue. Notification is attempted from the
coalescing work item, failure is counted, and no retry is performed.

### MTU requirement

A 48-byte notification requires ATT MTU 51 because a Handle Value
Notification has a 3-byte ATT header. The service performs a runtime check of
the active connection MTU before every notification and refuses oversized
frames. It also has a compile-time local L2CAP TX capacity assertion. This
keyboard/BlueZ pairing was measured with `btmon`: BlueZ requested 517, ZMK
responded 65, and the negotiated ATT MTU is 65, allowing a 62-byte payload.

## Build and automated checks

`CONFIG_ZMK_KEY_TELEMETRY` defaults off, depends on BLE and the split-central
role, and is enabled only by `config/corne_left.conf`. The central also enables
`CONFIG_ZMK_HID_INDICATORS` and
`CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING`. Both builds receive the
vendored module through `ZMK_EXTRA_MODULES`; Kconfig/CMake exclusion keeps all
telemetry code out of the right image.

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

The grep must find `CONFIG_ZMK_KEY_TELEMETRY=y` only on the left. The right
image must contain no telemetry objects, listeners, service identifiers, or
UUID byte sequence. See `docs/validation.md` for measured evidence.

## Manual BlueZ verification

Retain known-good images. Flash the right/peripheral half first and the
left/central half second, then remove and re-pair `Chocochap` because protocol
v2 changes the characteristic value shape. On a host with Bluetooth access:

```sh
python3 scripts/test-telemetry.py
python3 scripts/test-telemetry.py XX:XX:XX:XX:XX:XX
```

The verifier strictly decodes all v2 fields. Confirm the initial snapshot;
press/release and overlapping positions from both halves; complete layer masks;
home-row modifiers; Shift changing `/` to `?` in `kb-hud`; endpoint/profile,
battery, indicator, and split validity; sequence progression; and normal HID
operation. Live verification requires the host Bluetooth controller and system
BlueZ D-Bus.

If the UUID is missing, remove the device from BlueZ, clear or select the
corresponding ZMK profile, and pair again to invalidate the stale GATT cache.
Rollback is reflashing retained matching firmware/HUD versions; the protocol
changes no stored keymap data.
