# BLE Key Telemetry Specification

## Purpose
Define the optional encrypted BLE telemetry protocol, its central-only firmware placement, authoritative runtime state, and verification expectations.

## Requirements

### Requirement: Official external-module packaging
Telemetry SHALL reside in `modules/zmk-key-telemetry` as a self-contained Zephyr external module with its own `zephyr/module.yml`, Kconfig, CMake, headers, sources, and protocol tests. The option `CONFIG_ZMK_KEY_TELEMETRY` SHALL default to disabled and module sources SHALL be compiled only when that option is enabled.

#### Scenario: Discover the vendored module
- **WHEN** ZMK configures a build with `modules/zmk-key-telemetry` in `ZMK_EXTRA_MODULES`
- **THEN** Zephyr discovers the module through `zephyr/module.yml` and loads its Kconfig and CMake integration without modifying the official ZMK or Zephyr trees

#### Scenario: Leave telemetry disabled
- **WHEN** the module is discovered but `CONFIG_ZMK_KEY_TELEMETRY` is not enabled
- **THEN** no telemetry protocol or firmware implementation source is compiled into the application

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
Every read value and notification SHALL be exactly 20 bytes using protocol version `1`. It SHALL encode type, flags, global position, 16-bit sequence, 32-bit central monotonic timestamp, 32-bit complete active-layer mask, and 48-bit pressed-position bitmap at the documented fixed offsets using unsigned little-endian integers.

#### Scenario: Encode a physical key change
- **WHEN** a global physical position changes state
- **THEN** a type `0x02` record identifies that position, expresses press/release in flag bit zero, and includes the resulting complete layer and pressed-position state

#### Scenario: Encode a layer change
- **WHEN** the active layer state changes
- **THEN** a type `0x03` record uses position `0xff`, zero flags, and includes the complete resulting layer and pressed-position state

#### Scenario: Encode a snapshot
- **WHEN** the characteristic is read or a notification subscription is synchronized
- **THEN** a type `0x01` record uses position `0xff`, zero flags, and contains the current complete state without advancing the sequence

#### Scenario: Include the default layer
- **WHEN** the explicit ZMK layer-state bits do not contain the current default layer
- **THEN** the encoded active-layer mask still sets the default layer's bit

### Requirement: Native global physical positions
Telemetry SHALL report the official five-column layout's native global positions `0-35` for this keyboard. Bits `36-47` in the protocol bitmap SHALL remain clear, and firmware SHALL NOT translate events into the former 42-slot placeholder model.

#### Scenario: Press a left-half key
- **WHEN** a local matrix event is raised on the central half
- **THEN** telemetry reports the same global physical position used by the 36-entry keymap and `draw/corne.yaml`

#### Scenario: Press a right-half key
- **WHEN** normal ZMK split processing reconstructs a peripheral key event on the central
- **THEN** the central telemetry listener reports its global right-half position without telemetry code or a custom telemetry message on the peripheral

#### Scenario: Correlate presentation data
- **WHEN** a desktop overlay receives a set position bit and active-layer mask
- **THEN** it can obtain geometry, labels, bindings, and layer presentation from `draw/corne.yaml`, none of which is sent by the firmware protocol

### Requirement: Non-interfering event observation
Telemetry listeners SHALL observe position and layer events and SHALL always bubble them without consuming, delaying, rewriting, or synthesizing normal ZMK behavior. Telemetry availability or transmission failure SHALL NOT block HID or split processing.

#### Scenario: Process a subscribed key event
- **WHEN** telemetry observes a key event while a client is subscribed
- **THEN** it updates and queues telemetry state while allowing the original event to continue through normal ZMK processing

#### Scenario: Lose the host telemetry connection
- **WHEN** the BLE telemetry client disconnects or notification delivery fails
- **THEN** normal keyboard HID and split operation continues

### Requirement: Authoritative state and loss recovery
Every state revision SHALL advance the sequence modulo 65536 even when notifications are disabled or cannot be queued. Every emitted record SHALL repeat complete pressed-position and active-layer state. Notification delivery SHALL use bounded non-blocking work; overflow SHALL discard the oldest pending record rather than block keyboard processing.

#### Scenario: Detect a missed revision
- **WHEN** a client receives records whose modular sequence difference is not one
- **THEN** it can detect loss and replace its entire local state with the newest authoritative record

#### Scenario: Overflow the queue
- **WHEN** state changes outpace BLE notification delivery and the bounded queue fills
- **THEN** the oldest queued telemetry record is discarded and later authoritative records remain eligible for delivery

#### Scenario: Reestablish synchronization
- **WHEN** notifications are enabled, an encrypted connection is established, or the active BLE profile changes while the CCC is active
- **THEN** the firmware schedules a current snapshot as the client's new baseline

### Requirement: Protocol capacity and compatibility guards
The firmware SHALL fail its build if the keymap exceeds 48 positions, the keymap exceeds 32 layers, or the protocol record no longer fits the mandatory 23-byte ATT MTU. The fixed encoder SHALL have host-side tests for exact bytes and complete-layer behavior.

#### Scenario: Test the fixed encoder
- **WHEN** the host protocol test is run
- **THEN** known key, layer, and snapshot inputs produce the expected 20-byte protocol v1 values

#### Scenario: Exceed protocol limits
- **WHEN** a future configuration exceeds a declared position, layer, or ATT payload capacity
- **THEN** compile-time assertions fail rather than producing ambiguous telemetry
