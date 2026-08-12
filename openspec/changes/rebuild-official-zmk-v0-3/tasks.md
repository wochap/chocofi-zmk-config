## 1. Reproducible Project Foundation

- [x] 1.1 Add repository ignore rules for generated west projects, build directories, firmware artifacts, and local environments without ignoring source configuration or OpenSpec artifacts.
- [x] 1.2 Create `config/west.yml` with official ZMK and Zephyr full-commit pins, override ZMK's symbolic Zephyr import, and retain the pinned upstream import filters and modules.
- [x] 1.3 Create `flake.nix` with the required Nixpkgs URL, exact Zephyr source, pinned `nix-community/zephyr-nix`, SDK 0.16.8, and all declared local build/drawing tools.
- [x] 1.4 Generate and commit the genuinely updated `flake.lock`, then verify every declared flake input resolves to a locked commit and content hash.

## 2. Native Chocofi Configuration

- [x] 2.1 Add centralized layer IDs and a documented native 36-position header with left-hand, right-hand, thumb, and named combo positions.
- [x] 2.2 Add descriptive official ZMK hold-tap and macro definitions with the exact home-row, layer-tap, positional-trigger, and NAV-to-NUM semantics.
- [x] 2.3 Recreate Colemak-DH, QWERTY, NUM, NAV, FN, and ADJUST in exact layer-ID order using the official five-column Corne physical layout and all preserved finger/thumb bindings.
- [x] 2.4 Recreate the four editing combos with translated physical positions, base-layer restrictions, 18 ms timeout, and 150 ms prior-idle requirement.
- [x] 2.5 Add shared `corne.conf` settings for `Chocochap`, 30-minute deep sleep, +8 dBm transmit power, and experimental BLE connections, with no telemetry-specific half configuration.

## 3. Local Build and Drawing Workflow

- [x] 3.1 Add explicit Just recipes for west initialization/update, pristine left and right builds, combined builds, artifact copying, cleanup, and keymap drawing.
- [x] 3.2 Enter the locked Nix shell, initialize a fresh west workspace, and verify ZMK, Zephyr, and every enabled imported project resolve to immutable commits with the requested ZMK and Zephyr HEADs.
- [x] 3.3 Generate the six-layer 36-key YAML and SVG outputs with keymap-drawer and verify all four combos and the five-column physical layout are represented.

## 4. Firmware Verification

- [x] 4.1 Build the `nice_nano_v2` plus `corne_left` image pristinely and verify the UF2 artifact, `Chocochap` identity, central role, USB/BLE, sleep, transmit-power, and connection settings.
- [x] 4.2 Build the `nice_nano_v2` plus `corne_right` image pristinely and verify the UF2 artifact, peripheral role, BLE/split behavior, sleep, transmit-power, and connection settings.
- [x] 4.3 Inspect both compiled devicetrees to confirm every official hold-tap property and exact timing/trigger value is present, treating an unknown or absent property as a failed implementation.
- [x] 4.4 Compare all compiled layer bindings, thumb behaviors, macros, combos, controls, and layer IDs against the behavioral baseline and stop for direction if any typing behavior cannot be reproduced exactly.

## 5. Documentation and Final Audit

- [x] 5.1 Write the README with official v0.3.0 context, exact ZMK/Zephyr pins, Nix initialization, per-half and combined builds, artifact paths, drawing commands, and right-first flashing guidance.
- [x] 5.2 Document the native 36-position model and explicitly remove or avoid stale claims about urob's branch, telemetry requirements, GitHub Actions, mutable revisions, and obsolete workflows.
- [x] 5.3 Audit the final tree to confirm no CI implementation, `build.yaml`, telemetry module, `zmk-helpers`, `ZMK_EXTRA_MODULES`, mutable firmware revision, or generated west/build directory is included.
- [x] 5.4 Run the complete documented workflow from the clean repository state and record final validation results and any upstream-only warnings.
