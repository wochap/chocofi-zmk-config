# Local Firmware Workflow Specification

## Purpose
Define the local development, build, validation, drawing, and documentation workflow for producing both Chocofi firmware images.

## Requirements

### Requirement: Nix development shell
The repository SHALL provide a default Nix development shell containing all tools needed to initialize west, build both ARM firmware images, run the host telemetry protocol test, and regenerate the keymap drawing without relying on undeclared host development packages. It SHALL provide Python 3 and the BlueZ command-line client used by the manual verifier, while acknowledging that live verification also requires host Bluetooth and the system BlueZ service.

#### Scenario: Enter the shell
- **WHEN** a user runs `nix develop` in a supported Linux system
- **THEN** west, Git, CMake, Ninja, devicetree compiler, the ARM Zephyr SDK, a host C compiler, Python 3, `bluetoothctl`, Just, and keymap-drawer are available

### Requirement: Fresh west initialization
The local workflow SHALL document and provide a command that initializes west from `config/west.yml`, updates the pinned projects, and exports Zephyr from a clean repository checkout.

#### Scenario: Initialize a clean clone
- **WHEN** a user enters the Nix shell and runs the documented initialization command with no `.west` directory present
- **THEN** west creates the workspace and checks out the complete pinned dependency graph

### Requirement: Explicit per-half builds
The local workflow SHALL provide separate commands for `corne_left` and `corne_right` on `nice_nano_v2`, plus a command that builds both. Each build SHALL use `zmk/app` as its source, `config/` as `ZMK_CONFIG`, the vendored telemetry path as `ZMK_EXTRA_MODULES`, and a pristine or equivalently clean build directory.

#### Scenario: Build the left half
- **WHEN** the user invokes the left build command after initialization
- **THEN** the workflow produces a clearly named left/central UF2 artifact with telemetry enabled

#### Scenario: Build the right half
- **WHEN** the user invokes the right build command after initialization
- **THEN** the workflow produces a clearly named right/peripheral UF2 artifact with the telemetry implementation excluded

#### Scenario: Build all firmware
- **WHEN** the user invokes the combined build command
- **THEN** both per-half builds use the same explicit external-module path and both UF2 artifacts are placed in the documented firmware output directory

### Requirement: Keymap drawing workflow
The local workflow SHALL parse `config/corne.keymap` with keymap-drawer and regenerate a YAML model and SVG drawing using the official five-column Corne physical layout.

#### Scenario: Regenerate documentation drawing
- **WHEN** the user invokes the draw command in the Nix shell
- **THEN** `draw/corne.yaml` and `draw/corne.svg` represent all six 36-position layers and four combos

### Requirement: Build validation
The implementation SHALL validate both halves, resolved fork and Zephyr revisions, generated split-role and optional-state settings, compiled hold-tap properties, keymap-drawer output, telemetry protocol tests, central enablement, and peripheral binary exclusion before the change is considered complete.

#### Scenario: Validate a successful rebuild
- **WHEN** all implementation checks pass
- **THEN** both UF2 files exist, dependency HEADs match requested commits, optional telemetry sources are configured as designed, protocol-v2 tests pass, telemetry symbols and UUIDs exist on the left, and they are absent from the right

#### Scenario: Verify telemetry on hardware
- **WHEN** both halves are flashed and the verifier observes physical, layer, modifier, endpoint, battery, and split activity
- **THEN** 48-byte frames carry complete authoritative state, home-row modifier resolution is visible, sequences advance, and the negotiated ATT MTU is at least 51

#### Scenario: Encounter an unsupported optional source
- **WHEN** a proposed field lacks stable semantics or a supported API in the pinned fork
- **THEN** implementation leaves its validity bit clear and documents the limitation rather than modifying ZMK invasively

### Requirement: Local-build documentation
The README SHALL identify the ZMK modifier-hook fork, exact ZMK and Zephyr commits, Nix and west initialization commands, host telemetry test command, per-half and combined build commands, artifact locations, right-first flashing order, keymap drawing commands, and the optional central-only telemetry service. It SHALL link to protocol/manual verification documentation and SHALL NOT claim telemetry is required for ordinary HID use.

#### Scenario: Follow README from a clean clone
- **WHEN** a user follows the documented Nix initialization and per-half build steps
- **THEN** they can test protocol v2 and produce telemetry-enabled left and telemetry-free right firmware artifacts from immutable sources

### Requirement: Telemetry verification workflow
The repository SHALL provide a host-side fixed-protocol test and a manual BlueZ verifier. The script SHALL require only Python 3 and `bluetoothctl`, auto-detect one paired `Chocochap` or accept an explicit address, subscribe to the stable characteristic, reject invalid record length/version/declared size, and decode every protocol-v2 state field and mask.

#### Scenario: Run protocol unit tests
- **WHEN** the host test command runs
- **THEN** the protocol encoder compiles with warnings as errors and all exact 48-byte encoding, mask, sentinel, and layer tests pass

#### Scenario: Subscribe through BlueZ
- **WHEN** the verifier connects over an ATT MTU of at least 51
- **THEN** it prints decoded protocol-v2 snapshots and settled state frames including effective modifiers and optional valid state

#### Scenario: Run without container hardware access
- **WHEN** the environment lacks a Bluetooth controller or system BlueZ D-Bus access
- **THEN** automated protocol, build, and binary-isolation checks remain runnable and documentation identifies live BLE verification as a host-hardware step

### Requirement: No CI implementation
The repository SHALL NOT add a GitHub Actions workflow, another CI configuration, or a CI build matrix as part of this change.

#### Scenario: Inspect automation files
- **WHEN** the completed repository is reviewed
- **THEN** firmware building is provided as a local workflow and no CI implementation has been introduced
