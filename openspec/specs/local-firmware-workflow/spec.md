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
The implementation SHALL validate both halves, resolved dependency revisions, generated split-role and power settings, compiled hold-tap properties, keymap-drawer output, telemetry protocol tests, central enablement, and peripheral binary exclusion before the change is considered complete.

#### Scenario: Validate a successful rebuild
- **WHEN** all implementation checks pass
- **THEN** both UF2 files exist, dependency HEADs match the requested commits, required generated properties are present, the drawing contains the expected six layers and four combos, protocol tests pass, telemetry symbols and UUIDs exist on the left, and they are absent from the right

#### Scenario: Verify native split positions on hardware
- **WHEN** both halves are flashed and the telemetry verifier observes physical and layer activity
- **THEN** left and right keys appear as global positions in the native `0-35` range and layer records carry the complete six-layer mask

#### Scenario: Encounter an unsupported behavior
- **WHEN** official ZMK v0.3.0 cannot reproduce the telemetry protocol or baseline keyboard behavior exactly
- **THEN** implementation stops, records the old and new semantics and smallest compatible option, and requests user direction before changing wire or typing behavior

### Requirement: Local-build documentation
The README SHALL identify official ZMK v0.3.0, the exact ZMK and Zephyr commits, Nix and west initialization commands, host telemetry test command, per-half and combined build commands, artifact locations, right-first flashing order, keymap drawing commands, and the optional central-only telemetry service. It SHALL link to the protocol and manual verification guide and SHALL NOT claim that urob's going-modular branch, GitHub Actions, or telemetry is required for ordinary HID use.

#### Scenario: Follow README from a clean clone
- **WHEN** a user follows the documented Nix initialization and per-half build steps
- **THEN** they can test the protocol and produce telemetry-enabled left and telemetry-free right firmware artifacts without consulting the old repository

### Requirement: Telemetry verification workflow
The repository SHALL provide a host-side fixed-protocol test and SHALL copy `scripts/test-telemetry.py` as the manual BlueZ verifier. The script SHALL require only Python 3 and `bluetoothctl`, auto-detect a single paired `Chocochap` or accept an explicit Bluetooth address, subscribe to the stable characteristic UUID, reject invalid record length or version, and decode sequence, timestamp, event, layers, and pressed positions.

#### Scenario: Run protocol unit tests
- **WHEN** a user invokes the documented host test command in the Nix shell
- **THEN** the protocol encoder is compiled with warnings as errors and all fixed encoding and layer-mask tests pass

#### Scenario: Subscribe through BlueZ
- **WHEN** a user runs `python3 scripts/test-telemetry.py` with one paired `Chocochap` and accessible BlueZ hardware and D-Bus service
- **THEN** the script connects, selects the telemetry characteristic, enables notifications, and prints decoded protocol v1 records for keys and layers

#### Scenario: Run without container hardware access
- **WHEN** the NixOS container lacks a Bluetooth controller or system BlueZ D-Bus access
- **THEN** automated protocol, build, and binary-isolation checks remain runnable and documentation identifies live BLE verification as a host-hardware step

### Requirement: No CI implementation
The repository SHALL NOT add a GitHub Actions workflow, another CI configuration, or a CI build matrix as part of this change.

#### Scenario: Inspect automation files
- **WHEN** the completed repository is reviewed
- **THEN** firmware building is provided as a local workflow and no CI implementation has been introduced
