## Why

The current Chocofi configuration depends on obsolete `urob/zmk` development-branch wiring and does not reproducibly select the requested official ZMK and Zephyr sources. Rebuilding it cleanly on official ZMK v0.3.0 preserves the keyboard's established behavior while making both firmware images understandable, locally buildable, and immutably pinned.

## What Changes

- Recreate the west manifest around official ZMK commit `edf5c0814fd3ea202e43aad2d68fd32e882a518c` and ZMK's Zephyr fork commit `dacab4875df72109b96cc8977547a0dc04875bcd`.
- Ensure every firmware-affecting west project resolves to a full immutable commit, with release names used only in comments and documentation.
- Reimplement the six-layer Chocofi keymap with direct official ZMK v0.3.0 devicetree APIs, preserving bindings, layer IDs, macros, combos, timing, split roles, and controls.
- Recreate the urob-inspired home-row modifiers with official positional hold-tap properties and exact existing timing and modifier semantics.
- Model the 36-key hardware with ZMK's official five-column Corne physical layout, translating internal position IDs while preserving physical behavior and keymap-drawer output.
- Provide a pinned Nix development shell and straightforward local commands to initialize west and build either `nice_nano_v2` half.
- Replace stale urob-branch, telemetry, CI, and compatibility wiring with concise documentation and local-build tooling.
- Deliberately exclude telemetry and CI implementation from this change.

## Capabilities

### New Capabilities

- `chocofi-firmware-behavior`: Defines the split hardware roles, six-layer keymap, custom behaviors, controls, combos, and preserved typing semantics.
- `immutable-firmware-dependencies`: Defines immutable ZMK, Zephyr, west-project, Nix, and toolchain dependency requirements.
- `local-firmware-workflow`: Defines the Nix-based initialization, validation, drawing, and per-half local build workflow.

### Modified Capabilities

None. This repository has no existing OpenSpec capability specifications.

## Impact

- Adds a new clean configuration under `config/`, plus local build and keymap-drawing support.
- Adds `flake.nix` and a committed `flake.lock`; the baseline lock cannot be reused unchanged because it resolves the wrong Zephyr repository and commit.
- Produces separate left/central and right/peripheral UF2 images for two `nice_nano_v2` controllers.
- Changes internal physical position numbering from the legacy 42-slot Corne representation to a native 36-position representation. This does not change typing behavior; telemetry and other consumers of the old `0-41` position space are out of scope.
- Adds no GitHub Actions workflow, other CI implementation, telemetry module, or `zmk-helpers` dependency.
