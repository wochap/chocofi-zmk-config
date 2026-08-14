## Context

The behavioral source repository is a 36-key Chocofi configured as the stock ZMK `corne` split shield on two `nice_nano_v2` controllers. Its left half is the central, its right half is the peripheral, and its keymap uses six layers plus custom hold-taps, one layer-transition macro, and four combos. The existing repository also contains historical `urob/zmk` branch wiring and a later incomplete migration/telemetry experiment; neither is suitable as the new build architecture.

A disposable build established that the unchanged behavioral keymap compiles for both halves against official ZMK commit `edf5c0814fd3ea202e43aad2d68fd32e882a518c` and Zephyr commit `dacab4875df72109b96cc8977547a0dc04875bcd`. The official v0.3.0 hold-tap binding schema recognizes all required properties, and the compiled devicetree retains them.

The implementation repository is intentionally almost empty. This permits a clean reconstruction without copying obsolete workflows, generated workspaces, CI, or telemetry code.

## Goals / Non-Goals

**Goals:**

- Produce official ZMK v0.3.0 firmware for both Chocofi halves with no typing-behavior regression.
- Make every firmware source and toolchain input reproducible and auditable.
- Express the 36 physical keys and reusable behaviors clearly using direct ZMK devicetree.
- Provide documented, local Nix commands for initialization, building, and keymap drawing.
- Make ignored or unknown devicetree behavior properties visible as build failures through normal Zephyr binding validation and full pristine builds.

**Non-Goals:**

- Reintroducing the BLE telemetry module or preserving its historical `0-41` physical-position wire representation.
- Adding GitHub Actions, another CI system, or CI-oriented build matrix files.
- Adding ZMK Studio, displays, RGB behavior, `zmk-helpers`, or other functional enhancements.
- Changing timings, key assignments, combos, layer access, split roles, or the keyboard Bluetooth name.

## Decisions

### Use an official west graph with an explicit Zephyr override

`config/west.yml` will declare ZMK at the requested full commit and import its `app/west.yml` while blocklisting the imported `zephyr` project. It will then declare `zmkfirmware/zephyr` separately at the requested full commit and use the same project import filtering as the pinned ZMK manifest.

This retains ZMK's intended module set and paths while replacing its symbolic `v3.5.0+zmk-fixes` Zephyr revision. ZMK's imported nanopb and Studio message projects, and Zephyr's imported projects, remain governed by the immutable hashes in their pinned manifests. A flattened copy of those projects was considered but rejected because it would duplicate upstream manifest policy and make auditing future updates harder.

### Use the official five-column Corne physical layout

The keymap will select `foostan_corne_5col_layout` and contain 36 bindings per layer. Physical-position constants will be rewritten for the resulting `0-35` position space, including explicit left-hand, right-hand, and thumb groups.

Keeping the legacy 42-binding layout with six `&none` placeholders was considered because it preserves historical position IDs. It was rejected because no in-scope consumer uses those IDs, telemetry is excluded, and the official five-column layout models the actual hardware more clearly. Combo positions and hold-trigger groups will be translated by physical key, not copied numerically.

### Preserve behavior with direct official ZMK definitions

The keymap will use normal devicetree behavior nodes and small C-preprocessor aliases only where they remove repeated multi-binding sequences. It will define:

- separate descriptive left and right home-row hold-taps;
- one descriptive layer-tap behavior for Tab/NUM, Enter/NAV, and Escape/NAV;
- the existing macro that releases NAV before momentarily activating NUM;
- four named combos with explicit layer, timeout, and idle restrictions.

Layer IDs will be centralized in one header. Physical position groups will be centralized in another. Reusable behavior nodes may live in a local `.dtsi`; layer bindings remain in `corne.keymap`. `zmk-helpers` is unnecessary because every required feature exists in official v0.3.0.

### Keep the stock Corne split definition

Both images will target `nice_nano_v2` with `corne_left` or `corne_right`. The stock shield makes the left half central and sets the right transform's column offset. Shared `corne.conf` will retain `Chocochap`, deep sleep after 30 minutes, +8 dBm controller power, and the existing experimental BLE connection option. No telemetry-specific `corne_left.conf` will be created.

### Use a small Nix-first local workflow

`flake.nix` will use the exact requested Nixpkgs URL and a full-commit-pinned `nix-community/zephyr-nix`. Its Zephyr source input will point at the same `dacab4875df72109b96cc8977547a0dc04875bcd` fork commit used by west, so the Python environment matches the firmware base. The shell will expose Zephyr SDK 0.16.8 for ARM, west and its Python dependencies, CMake, devicetree compiler, Ninja, Git, Just, and keymap-drawer.

The baseline `flake.lock` resolves a different upstream Zephyr source, so a lock update is genuinely required. The resulting lock will be committed. `flake-utils`, the old `urob/zephyr-nix` input, QEMU, yq-based build-matrix parsing, and CI `build.yaml` are unnecessary and will not be introduced.

The Justfile will offer explicit initialization, left build, right build, combined build, clean, and draw recipes. Explicit recipes are clearer for two fixed targets than a generic CI matrix parser.

### Validate source selection and behavior at build time

Verification will include:

- resolving the west manifest and checking project revisions are full hashes;
- confirming the checked-out ZMK and Zephyr HEADs equal the requested commits;
- pristine builds of both halves;
- checking generated Kconfig for `Chocochap`, left-central/right-peripheral roles, sleep, BLE, and transmit-power settings;
- checking the compiled devicetree contains all exact home-row hold-tap properties and values;
- parsing and drawing the 36-position keymap with keymap-drawer;
- comparing every layer row, thumb binding, combo, and control against the behavioral baseline.

Known warnings originating in the pinned upstream board configuration are not equivalent to unknown devicetree properties. Unknown behavior properties must fail devicetree binding validation or be detected as absent from the generated devicetree.

## Risks / Trade-offs

- **Internal position IDs change from 42-slot to 36-slot numbering** → Translate combos and trigger groups by named physical position, regenerate the drawing, and keep telemetry out of scope until a future protocol decision is made.
- **A symbolic revision can enter through an imported manifest** → Inspect the resolved west manifest after initialization and fail verification if any firmware project does not resolve to a 40-character commit.
- **The old Nix lock cannot be preserved byte-for-byte** → Limit the lock change to the clean flake's declared, fully pinned inputs and document why the Zephyr source change requires it.
- **A successful build cannot prove radio pairing or typing feel** → Build and statically compare both images, then document flashing the right half first and retaining the old firmware as a rollback image for hardware testing.
- **Upstream v0.3.0 emits board/Kconfig deprecation warnings** → Do not suppress them or confuse them with devicetree validation; record them if present while requiring successful builds and exact generated settings.

## Migration Plan

1. Create the manifest, Nix shell, local recipes, and ignore rules in the clean repository.
2. Recreate the shared configuration and native 36-position keymap from the behavioral source.
3. Initialize a fresh west workspace and verify every resolved revision.
4. Build and inspect both firmware images, then regenerate the keymap drawing.
5. Update the README with exact pins, initialization, per-half builds, flashing order, and the telemetry/CI exclusions.
6. For device validation, retain the last known firmware, flash the right/peripheral image first, then the left/central image, and exercise base typing, hold-taps, layer chords, combos, profile selection, and output controls. Roll back by reflashing the retained images if a hardware-only regression appears.

## Open Questions

None. The proposal adopts native 36-position numbering; any future telemetry work must explicitly define how it maps that numbering rather than inheriting the old protocol implicitly.
