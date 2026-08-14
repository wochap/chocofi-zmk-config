# BLE Key Telemetry Specification

## Purpose
Define the optional encrypted BLE telemetry protocol, its central-only firmware placement, authoritative runtime state, and verification expectations.

## Requirements

### Requirement: Official external-module packaging
The standalone `zmk-key-telemetry` repository SHALL contain a self-contained Zephyr module at its root with `zephyr/module.yml`, Kconfig, CMake, headers, sources, protocol tests, module documentation, and the authoritative telemetry OpenSpec specification. `CONFIG_ZMK_KEY_TELEMETRY` SHALL default to disabled, and module sources SHALL compile only when enabled.

#### Scenario: Discover the standalone module
- **WHEN** ZMK configures a build with the west-managed `zmk-key-telemetry` checkout in `ZMK_EXTRA_MODULES`
- **THEN** Zephyr discovers the module and exposes `CONFIG_ZMK_KEY_TELEMETRY`

#### Scenario: Leave telemetry disabled
- **WHEN** the module is present and `CONFIG_ZMK_KEY_TELEMETRY` is not enabled
- **THEN** no telemetry protocol or firmware implementation source is compiled

#### Scenario: Inspect repository ownership
- **WHEN** both repositories are inspected
- **THEN** reusable module code, protocol tests, module documentation, and its authoritative specification exist only in `zmk-key-telemetry`

### Requirement: Central-only telemetry placement
For a split keyboard, `CONFIG_ZMK_KEY_TELEMETRY` SHALL depend on BLE and the split-central role. `config/corne_left.conf` SHALL enable it, and the right/peripheral configuration SHALL NOT enable it.

#### Scenario: Build the left central
- **WHEN** `nice_nano_v2` is built with `corne_left`
- **THEN** the generated configuration contains `CONFIG_ZMK_KEY_TELEMETRY=y` and the firmware contains the telemetry service and event listeners

#### Scenario: Build the right peripheral
- **WHEN** `nice_nano_v2` is built with `corne_right` while the module remains discoverable
- **THEN** the generated configuration does not enable telemetry and the firmware contains no telemetry implementation, listener registration, GATT service, or telemetry UUID

### Requirement: Encrypted GATT service
The central firmware SHALL expose service UUID `9e7a7d70-df1b-4f76-9d45-8c3f4a6b2100` with one read-and-notify record characteristic at UUID `9e7a7d70-df1b-4f76-9d45-8c3f4a6b2101`. Characteristic reads and CCC reads/writes SHALL require an encrypted BLE link, and the service UUID SHALL NOT be added to advertising data.

#### Scenario: Use a bonded encrypted connection
- **WHEN** a normally paired client connects, discovers the record characteristic, reads it, and enables notifications
- **THEN** the read succeeds and the client can receive telemetry notifications

#### Scenario: Attempt unencrypted access
- **WHEN** a client without link encryption reads the characteristic or writes its CCC
- **THEN** the GATT operation is denied by the encrypted permissions

### Requirement: Protocol v1 record encoding
Every characteristic read and notification SHALL be exactly 48 bytes using protocol version `2`. It SHALL encode frame flags, declared frame size, 32-bit sequence, 64-bit central monotonic timestamp, 64-bit pressed-position bitmap, 32-bit complete active-layer mask, 32-bit changed-field mask, 32-bit valid-field mask, effective HID modifiers, HID indicators, default layer, selected transport, BLE profile, central and peripheral battery percentages, split status, and 32-bit cumulative dropped-frame count at documented fixed offsets using unsigned little-endian integers.

#### Scenario: Encode a settled state revision
- **WHEN** one or more observed keyboard-state fields settle after synchronous ZMK processing
- **THEN** one non-snapshot frame contains the complete resulting state and marks every coalesced cause in `changed_fields`

#### Scenario: Encode a snapshot
- **WHEN** the characteristic is read or a notification subscription is synchronized
- **THEN** the frame sets snapshot flag bit zero, uses `changed_fields == 0`, and contains the current complete state without advancing the sequence

#### Scenario: Represent unavailable optional state
- **WHEN** a state source is not enabled or authoritative in the current build
- **THEN** its `valid_fields` bit is clear and its value uses the documented zero or `0xff` sentinel

#### Scenario: Include the default layer
- **WHEN** the explicit ZMK layer-state bits do not contain the current default layer
- **THEN** the encoded active-layer mask still sets the default layer's bit and the default-layer field identifies it

### Requirement: Native global physical positions
Telemetry SHALL report the official five-column layout's native global positions `0-35` in a 64-bit bitmap. Bits `36-63` SHALL remain clear, and firmware SHALL NOT translate events into the former 42-slot placeholder model.

#### Scenario: Press a left-half key
- **WHEN** a local matrix event is raised on the central half
- **THEN** the next settled frame reports the same global physical position used by the 36-entry keymap and `draw/corne.yaml`

#### Scenario: Press a right-half key
- **WHEN** normal ZMK split processing reconstructs a peripheral key event on the central
- **THEN** the next settled frame reports its global right-half position without telemetry code or a custom telemetry message on the peripheral

#### Scenario: Correlate presentation data
- **WHEN** a desktop overlay receives pressed positions, layers, and modifiers
- **THEN** it can obtain geometry, labels, bindings, shifted-label presentation, and hold-label correlations from desktop-side keymap data, none of which is sent by firmware

### Requirement: Non-interfering event observation
Telemetry listeners SHALL observe position, layer, effective-modifier, endpoint, HID-indicator, battery, and peripheral-battery events and SHALL always bubble them without consuming, delaying, rewriting, or synthesizing normal ZMK behavior. Listeners SHALL only update small module-owned state, mark dirty fields, and schedule deferred coalesced work; telemetry availability or transmission failure SHALL NOT block HID or split processing.

#### Scenario: Process synchronous cascaded events
- **WHEN** one logical home-row operation produces position and effective-modifier changes
- **THEN** listeners mark both fields dirty and deferred work emits one authoritative frame after synchronous behavior processing settles

#### Scenario: Observe effective modifiers
- **WHEN** explicit, implicit, masked, or report-clear behavior changes the effective HID modifier byte
- **THEN** `zmk_hid_modifiers_changed` marks modifiers dirty and the deferred builder samples the complete live HID report byte

#### Scenario: Lose the host telemetry connection
- **WHEN** the BLE telemetry client disconnects, has an insufficient negotiated MTU, or notification delivery fails
- **THEN** normal keyboard HID and split operation continues

### Requirement: Authoritative state and loss recovery
Every coalesced settled state revision SHALL advance a 32-bit sequence modulo 2^32 even when notifications are disabled or cannot be sent. Every emitted frame SHALL repeat complete authoritative state. Deferred work SHALL preserve changes that arrive while a frame is built. Attempted frames that cannot be delivered SHALL increase a cumulative dropped-frame counter without retrying or blocking keyboard processing.

#### Scenario: Detect a missed revision
- **WHEN** a client receives non-snapshot frames whose modular sequence difference is not one
- **THEN** it can detect loss and replace its entire local state with the newest authoritative frame

#### Scenario: Observe firmware-side loss
- **WHEN** a notification attempt fails after a frame sequence has been assigned
- **THEN** the dropped-frame counter increments and is included in the next successfully emitted frame

#### Scenario: Reestablish synchronization
- **WHEN** notifications are enabled, an encrypted connection is established, or the active BLE profile changes while the CCC is active
- **THEN** firmware schedules a current snapshot as the client's new baseline

### Requirement: Protocol capacity and compatibility guards
Firmware SHALL fail its build if the keymap exceeds 64 positions, the keymap exceeds 32 layers, the encoded layout is not exactly 48 bytes, or the frame exceeds the locally configured ATT/L2CAP notification capacity. At runtime it SHALL attempt notification only when the negotiated ATT MTU is at least 51 bytes. The fixed encoder SHALL have host tests for exact bytes, complete-layer behavior, masks, sentinels, and boundary values.

#### Scenario: Test the fixed encoder
- **WHEN** the host protocol test is run
- **THEN** known state and snapshot inputs produce expected 48-byte protocol-v2 values at every documented offset

#### Scenario: Negotiate sufficient MTU
- **WHEN** `bt_gatt_get_mtu(conn)` returns at least 51
- **THEN** a 48-byte notification is eligible for delivery

#### Scenario: Negotiate insufficient MTU
- **WHEN** `bt_gatt_get_mtu(conn)` returns less than 51
- **THEN** firmware does not call the notification API for that frame and continues normal keyboard operation

#### Scenario: Exceed protocol limits
- **WHEN** a future configuration exceeds a declared position, layer, encoded-size, or local-transport capacity
- **THEN** compile-time assertions fail rather than producing ambiguous telemetry

### Requirement: Authoritative optional keyboard state
The central telemetry build SHALL enable and report supported host HID indicators and split-peripheral battery fetching. It SHALL report endpoint/profile and local/peripheral batteries from supported public central-side ZMK APIs/events. It SHALL keep split status invalid in this pinned version because ZMK exposes no authoritative public central-side status source, and SHALL use `valid_fields` instead of inventing unavailable values.

#### Scenario: Change output endpoint
- **WHEN** ZMK selects USB or a BLE profile
- **THEN** the settled frame identifies the selected transport and profile and marks endpoint state valid

#### Scenario: Update battery state
- **WHEN** local or right-peripheral battery state changes
- **THEN** the settled frame carries the percentage and corresponding validity/change bits

#### Scenario: Update host indicators
- **WHEN** the selected host changes Caps Lock, Num Lock, Scroll Lock, Compose, or Kana indicators
- **THEN** the settled frame carries ZMK's complete indicator byte

#### Scenario: Represent unavailable right-half connection state
- **WHEN** this pinned central firmware constructs a frame
- **THEN** the split-status validity bit is clear and its value is the unknown sentinel
