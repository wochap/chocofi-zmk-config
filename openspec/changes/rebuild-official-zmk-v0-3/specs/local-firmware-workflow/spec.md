## ADDED Requirements

### Requirement: Nix development shell
The repository SHALL provide a default Nix development shell containing all tools needed to initialize west, build both ARM firmware images, and regenerate the keymap drawing without relying on undeclared host development packages.

#### Scenario: Enter the shell
- **WHEN** a user runs `nix develop` in a supported Linux system
- **THEN** west, Git, CMake, Ninja, devicetree compiler, the ARM Zephyr SDK, Just, and keymap-drawer are available

### Requirement: Fresh west initialization
The local workflow SHALL document and provide a command that initializes west from `config/west.yml`, updates the pinned projects, and exports Zephyr from a clean repository checkout.

#### Scenario: Initialize a clean clone
- **WHEN** a user enters the Nix shell and runs the documented initialization command with no `.west` directory present
- **THEN** west creates the workspace and checks out the complete pinned dependency graph

### Requirement: Explicit per-half builds
The local workflow SHALL provide separate commands for `corne_left` and `corne_right` on `nice_nano_v2`, plus a command that builds both. Each build SHALL use `zmk/app` as its source, `config/` as `ZMK_CONFIG`, and a pristine or equivalently clean build directory.

#### Scenario: Build the left half
- **WHEN** the user invokes the left build command after initialization
- **THEN** the workflow produces a clearly named left/central UF2 artifact

#### Scenario: Build the right half
- **WHEN** the user invokes the right build command after initialization
- **THEN** the workflow produces a clearly named right/peripheral UF2 artifact

#### Scenario: Build all firmware
- **WHEN** the user invokes the combined build command
- **THEN** both per-half builds run and both UF2 artifacts are placed in the documented firmware output directory

### Requirement: Keymap drawing workflow
The local workflow SHALL parse `config/corne.keymap` with keymap-drawer and regenerate a YAML model and SVG drawing using the official five-column Corne physical layout.

#### Scenario: Regenerate documentation drawing
- **WHEN** the user invokes the draw command in the Nix shell
- **THEN** `draw/corne.yaml` and `draw/corne.svg` represent all six 36-position layers and four combos

### Requirement: Build validation
The implementation SHALL validate both halves, resolved dependency revisions, generated split-role and power settings, compiled hold-tap properties, and keymap-drawer output before the change is considered complete.

#### Scenario: Validate a successful rebuild
- **WHEN** all implementation checks pass
- **THEN** both UF2 files exist, dependency HEADs match the requested commits, required generated properties are present, and the drawing contains the expected six layers and four combos

#### Scenario: Encounter an unsupported behavior
- **WHEN** official ZMK v0.3.0 cannot reproduce a baseline behavior exactly
- **THEN** implementation stops, records the old and new semantics and smallest compatible option, and requests user direction before changing typing behavior

### Requirement: Local-build documentation
The README SHALL identify official ZMK v0.3.0, the exact ZMK and Zephyr commits, Nix and west initialization commands, per-half and combined build commands, artifact locations, right-first flashing order, and keymap drawing commands. It SHALL NOT claim that urob's going-modular branch, GitHub Actions, or telemetry is required.

#### Scenario: Follow README from a clean clone
- **WHEN** a user follows the documented Nix initialization and per-half build steps
- **THEN** they can produce the left and right firmware artifacts without consulting the old repository

### Requirement: No CI implementation
The repository SHALL NOT add a GitHub Actions workflow, another CI configuration, or a CI build matrix as part of this change.

#### Scenario: Inspect automation files
- **WHEN** the completed repository is reviewed
- **THEN** firmware building is provided as a local workflow and no CI implementation has been introduced
