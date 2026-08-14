# Immutable Firmware Dependencies Specification

## Purpose
Define the immutable source revisions, Nix inputs, toolchain, and module constraints required for reproducible firmware builds.

## Requirements

### Requirement: Exact ZMK source
The west manifest SHALL fetch ZMK from `https://github.com/wochap/zmk` at full commit `a301f6d562bd67f18e496402f8cf6c87326b05b2` containing the snapshot-style `zmk_hid_modifiers_changed` contract. The corresponding `v0.3-branch-fork` name SHALL appear only in comments and documentation, never as the effective revision.

#### Scenario: Resolve ZMK
- **WHEN** west resolves and updates the manifest
- **THEN** the ZMK project HEAD is exactly `a301f6d562bd67f18e496402f8cf6c87326b05b2` and contains `app/include/zmk/events/hid_modifiers_changed.h`

### Requirement: Exact Zephyr source
The west manifest and Nix Zephyr source input SHALL use `https://github.com/zmkfirmware/zephyr` at full commit `dacab4875df72109b96cc8977547a0dc04875bcd`. The corresponding `v3.5.0+zmk-fixes` name SHALL appear only in comments and documentation, never as the effective revision.

#### Scenario: Resolve Zephyr with west
- **WHEN** west resolves and updates the manifest
- **THEN** the Zephyr project HEAD is exactly `dacab4875df72109b96cc8977547a0dc04875bcd`

#### Scenario: Resolve Zephyr with Nix
- **WHEN** Nix evaluates the development shell
- **THEN** the Zephyr source used to construct its Python environment resolves to `dacab4875df72109b96cc8977547a0dc04875bcd`

### Requirement: Immutable imported west projects
Every firmware-affecting project in the resolved west manifest SHALL have a 40-character commit revision. The configuration SHALL override the symbolic Zephyr revision imported by ZMK without copying mutable branch or tag names into effective build revisions.

#### Scenario: Freeze the resolved manifest
- **WHEN** the initialized workspace is inspected with west's manifest tooling
- **THEN** ZMK, Zephyr, nanopb, ZMK Studio messages, and all enabled Zephyr imports resolve to immutable commit hashes

#### Scenario: Detect a mutable revision
- **WHEN** a resolved firmware project is configured with `main`, `master`, a release branch, or a tag instead of a commit
- **THEN** dependency verification fails before firmware is accepted

### Requirement: Pinned Nix inputs
`flake.nix` SHALL use `nixpkgs.url = "github:nixos/nixpkgs?rev=0ad6f47ea4fe188f4bc8f0380f93ae8523337c6c"` with the comment `# nixos-26.05 (10 jul 2026)`. Other non-local flake inputs SHALL be selected immutably through full commit URLs and the committed lock file.

#### Scenario: Inspect flake metadata
- **WHEN** Nix resolves the flake
- **THEN** Nixpkgs resolves to `0ad6f47ea4fe188f4bc8f0380f93ae8523337c6c` and every other declared input has a locked commit and content hash

### Requirement: Committed lock file
The repository SHALL commit `flake.lock`. Recreating the baseline lock SHALL be permitted for this change because the baseline resolves upstream Zephyr commit `a6eef0ba3755f2530c5ce93524e5ac4f5be30194`, which does not satisfy the requested ZMK Zephyr fork pin.

#### Scenario: Enter the development shell from a clean clone
- **WHEN** a user runs `nix develop` with the committed lock file
- **THEN** Nix uses the recorded dependency graph without selecting newer mutable revisions

### Requirement: Compatible pinned toolchain
The Nix development shell SHALL provide an ARM Zephyr SDK 0.16.8 toolchain and the Python dependencies required by Zephyr 3.5.0 and west. The toolchain provider SHALL itself be pinned through the flake and lock file.

#### Scenario: Configure a firmware build
- **WHEN** CMake configures either half inside `nix develop`
- **THEN** it finds Zephyr SDK 0.16.8 and a west version compatible with the pinned Zephyr release

### Requirement: No unnecessary firmware modules
The dependency graph SHALL NOT add `zmk-helpers` or another third-party ZMK behavior module. The first-party telemetry implementation SHALL remain vendored at `modules/zmk-key-telemetry`, SHALL be discovered through explicit `ZMK_EXTRA_MODULES` build wiring, and SHALL NOT be added as a west project or copied into the pinned ZMK or Zephyr source tree. All telemetry integration SHALL consume public APIs and the modifier event present in the pinned fork.

#### Scenario: Inspect the resolved project list
- **WHEN** west lists all resolved projects
- **THEN** no helper or telemetry project appears and ZMK and Zephyr resolve to their exact requested commits

#### Scenario: Inspect module discovery
- **WHEN** either firmware build command is inspected
- **THEN** it passes only the vendored `modules/zmk-key-telemetry` path through `ZMK_EXTRA_MODULES`

#### Scenario: Inspect ownership boundaries
- **WHEN** telemetry and keyboard sources are reviewed
- **THEN** reusable telemetry implementation is confined to its external module, keyboard-specific enablement is confined to configuration, and the initialized ZMK and Zephyr worktrees remain unmodified
