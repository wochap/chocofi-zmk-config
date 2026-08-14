# Validation record

Validation was completed on 12 August 2026 in the locked Nix shell.

## Dependency resolution

- `flake.lock` contains seven non-root inputs; each has a 40-character commit
  and a Nix content hash.
- The development shell reports West 1.2.0, Zephyr SDK 0.16.8, and
  keymap-drawer 0.23.0.
- A fresh `just init` resolved 39 active west projects. Every declared
  revision was a 40-character commit and every checkout HEAD matched it.
- ZMK HEAD was `edf5c0814fd3ea202e43aad2d68fd32e882a518c`.
- Zephyr HEAD was `dacab4875df72109b96cc8977547a0dc04875bcd`.

## Firmware builds

`just build-all` performed pristine official-ZMK builds for both targets.

| Target | Role | UF2 size | SHA-256 |
| --- | --- | ---: | --- |
| `nice_nano_v2` + `corne_left` | central | 440832 bytes | `7cddb0859f603996d3423861d7292a0aa8538a683bdbed26ac96e38350146b1f` |
| `nice_nano_v2` + `corne_right` | peripheral | 346112 bytes | `d140d01b491e5795071dea57ee51d079de0dc20c0fb987ca969dedd80292eb32` |

Generated Kconfig was checked for `Chocochap`, split role, USB/BLE as
appropriate to the role, 30-minute sleep, +8 dBm transmit power, and the
experimental BLE connection option.

## Behavior audit

The official v0.3.0 `zmk,behavior-hold-tap` binding schema contains every
property used by this configuration. Both compiled devicetrees retain:

- balanced home-row hold-taps with 275 ms tapping term, 150 ms quick tap,
  150 ms prior-idle requirement, opposite-hand plus thumb triggers, and
  `hold-trigger-on-release`;
- the hold-preferred layer-tap with 125 ms tapping term and 150 ms quick tap;
- the NAV-to-NUM macro sequence and 1 ms wait;
- all four combos on layers 0 and 1 with 18 ms timeout and 150 ms prior-idle
  requirement;
- layer nodes in the required `0-5` order and the official five-column Corne
  physical layout.

The generated 36-key YAML was compared with the behavioral repository's
42-slot output after removing only the six unused outer-column placeholders
and translating physical positions. All finger keys, thumb bindings,
home-row modifiers, layer access, macro placement, Bluetooth controls,
USB/BLE controls, external-power controls, bootloader/reset controls, and
combo bindings matched.

## Upstream warnings

The pinned upstream stack emits two warnings which do not indicate ignored
devicetree properties:

- both halves enable Zephyr's deprecated
  `NRF_STORE_REBOOT_TYPE_GPREGRET` through the upstream board configuration;
- the right/peripheral build reports that upstream's requested `ZMK_USB=y` is
  resolved to `n`, because USB is available only when the split role is
  central. The left/central build retains USB.

No unknown devicetree property warning was emitted, and every required
hold-tap property was present in both generated devicetrees.
