# Chocofi ZMK configuration

This repository builds firmware for a 36-key low-profile Chocofi with two
`nice_nano_v2` controllers. It uses official ZMK v0.3.0 directly: the left
half is the split central and the right half is the split peripheral. The
Bluetooth name is `Chocochap`.

The project is intentionally local-only. It has no CI build matrix and no
telemetry module.

## Locked firmware base

Release names below identify the corresponding upstream release for humans;
builds use only the full commit hashes.

| Component | Release | Effective revision |
| --- | --- | --- |
| ZMK | v0.3.0 | `edf5c0814fd3ea202e43aad2d68fd32e882a518c` |
| ZMK Zephyr fork | v3.5.0+zmk-fixes | `dacab4875df72109b96cc8977547a0dc04875bcd` |
| Nixpkgs | nixos-26.05 snapshot (10 July 2026) | `0ad6f47ea4fe188f4bc8f0380f93ae8523337c6c` |
| nix-community/zephyr-nix | SDK/Python packaging | `a12131ec450ea66e9005c668c31c8a055a766ef3` |

`config/west.yml` pins ZMK and overrides its symbolic Zephyr import with the
exact Zephyr commit above. Imported west projects remain pinned by the
immutable manifests at those commits. `flake.lock` fixes every Nix input and
content hash. The development shell provides West 1.2.0, Zephyr SDK 0.16.8,
and keymap-drawer 0.23.0.

## Initialize locally with Nix

Install Nix with flakes enabled, clone this repository, and run:

```sh
nix develop
just init
```

`just init` initializes the repository as a west workspace, checks out the
pinned projects, and exports Zephyr's CMake package. Run `just update` later to
restore the same declared immutable revisions; it does not advance them.

## Build firmware

Enter `nix develop`, then build either image independently:

```sh
just build-left
just build-right
```

Or build both with:

```sh
just build-all
```

Every recipe requests a pristine build for `nice_nano_v2`. The copied output
files are:

- `firmware/corne_left-nice_nano_v2.uf2` — central, host-facing half
- `firmware/corne_right-nice_nano_v2.uf2` — peripheral half

`just clean` removes only build and copied firmware outputs.

## Flashing

Keep a copy of the last known-good firmware as a recovery baseline. Flash the
right/peripheral image first, then flash the left/central image. Put each
nice!nano into its UF2 bootloader and copy the matching file to the mounted
bootloader drive.

After flashing, verify base typing, home-row holds, thumb layer-taps, layer
transitions, editing combos, Bluetooth profiles, and USB/BLE output selection.
If hardware-only behavior differs, reflash the retained recovery images.

## Keymap structure

Layer IDs are centralized in `config/layers.h` and remain:

| ID | Layer |
| ---: | --- |
| 0 | Colemak-DH |
| 1 | QWERTY |
| 2 | NUM |
| 3 | NAV |
| 4 | FN |
| 5 | ADJUST |

The keymap selects
official ZMK's `foostan_corne_5col_layout`, so every layer has the native 36
positions documented in `config/key_positions.h`. The historical six unused
outer-column placeholders are gone; combos and positional hold triggers are
translated by physical key into the `0-35` position space.

Reusable home-row, layer-tap, and macro behaviors live in
`config/custom_behaviors.dtsi`; layer bindings and combos remain readable in
`config/corne.keymap`. All behavior definitions use official ZMK devicetree
APIs without helper modules.

Regenerate the committed keymap-drawer outputs with:

```sh
just draw
```

This writes `draw/corne.yaml` and `draw/corne.svg`. See
`docs/validation.md` for the build and static-equivalence audit.
