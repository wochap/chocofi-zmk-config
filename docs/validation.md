# Validation record

Validation was completed on 13 August 2026 in the locked development
environment. Live flashing remains a separate hardware step.

## Protocol and consumer checks

- `just test` compiled the explicit protocol-v2 encoder as C11 with
  `-Wall -Wextra -Werror -pedantic`; every exact-byte, offset, mask, sentinel,
  layer, timestamp, sequence, and 64-position-boundary assertion passed.
- `python3 -m py_compile scripts/test-telemetry.py` passed. The verifier now
  requires exactly 48 bytes, version 2, declared size 48, known flags, valid
  enum combinations, valid percentages, and validity-aware sentinels.
- In `kb-hud`, Vitest passed all 38 frontend/overlay tests and the TypeScript +
  Vite production build passed.
- In the `kb-hud` Nix shell, all 14 Rust decoder/state/config tests passed.
  The modified Rust sources were formatted with the shell's `rustfmt`.
- Both repositories' `telemetry-state-frame-v2` OpenSpec changes pass strict
  validation.

## Dependency and API audit

`config/west.yml` and the local checkout resolve exactly to:

| Component | Revision |
| --- | --- |
| `wochap/zmk` | `a301f6d562bd67f18e496402f8cf6c87326b05b2` |
| ZMK Zephyr fork | `dacab4875df72109b96cc8977547a0dc04875bcd` |
| `wochap/zmk-key-telemetry` | `3112fba167f1c97a04babdf5659eb1c53464a0ef` |

The checkouts were clean after pristine builds. The ZMK fork contains and
compiles `zmk_hid_modifiers_changed`; `hid.c` raises its complete effective byte
only after updating `keyboard_report.body.modifiers`. Telemetry samples that
same report through `zmk_hid_get_keyboard_report()` after deferred coalescing.

The following public sources were verified in this exact checkout and compiled
in the left image:

- `zmk_keymap_layer_state()` and `zmk_keymap_layer_default()`;
- `zmk_endpoints_selected()` and `zmk_ble_active_profile_conn()`;
- `zmk_hid_indicators_get_current_profile()`;
- `zmk_battery_state_of_charge()`;
- central-side `zmk_peripheral_battery_state_changed` events when split battery
  fetching is enabled.

The proposed split connection field does not have a usable central-side source
in this pinned ZMK revision. `zmk_split_peripheral_status_changed` is raised by
the split-peripheral Bluetooth implementation, while telemetry runs only on the
central. The implementation therefore leaves bit 8 of `valid_fields` clear and
encodes the unknown status sentinel. It does not add an invasive internal
central connection hook merely to populate optional status.

## Firmware builds

Both targets were rebuilt pristine from the pinned fork.

| Target | Role | Flash used | RAM used | UF2 size | SHA-256 |
| --- | --- | ---: | ---: | ---: | --- |
| `corne_left` | central + telemetry v2 | 224080 B | 52720 B | 448512 B | `ffc0f69ad531c0ff200fa00c5919770995e802cf80ca2c9e2f95efed124c9a45` |
| `corne_right` | split peripheral | 172880 B | 32508 B | 346112 B | `d140d01b491e5795071dea57ee51d079de0dc20c0fb987ca969dedd80292eb32` |

The left generated configuration contains:

```text
CONFIG_ZMK_HID_INDICATORS=y
CONFIG_ZMK_SPLIT_ROLE_CENTRAL=y
CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING=y
CONFIG_ZMK_KEY_TELEMETRY=y
CONFIG_ZMK_KEY_TELEMETRY_COALESCE_MS=1
CONFIG_BT_L2CAP_TX_MTU=65
```

The right configuration contains `CONFIG_BT_L2CAP_TX_MTU=65` but no telemetry,
central-role, HID-indicator, or central split-battery-fetch assignment. Its
compile database contains neither telemetry source. Its ELF contains no
telemetry symbol/string, and neither stable UUID byte sequence appears in its
copied UF2. Both UUIDs and all telemetry listener/encoder symbols are present
in the left image.

The compile-time capacity assertion proves the 48-byte frame plus 3-byte ATT
header fits the configured local TX MTU of 65. The runtime path separately
checks `bt_gatt_get_mtu(conn) >= 51` before every notification. External
`btmon` verification established a negotiated ATT MTU of 65 (62-byte
notification value capacity) for the intended BlueZ connection.

## Hardware verification status

No live flashing or key interaction is claimed by this repository-only run.
On accessible hardware:

1. Retain matching known-good firmware and HUD builds.
2. Flash the right/peripheral image first, then the left/central image.
3. Remove and re-pair `Chocochap` if BlueZ retained the old characteristic
   value shape in its GATT cache.
4. Run `python3 scripts/test-telemetry.py`; confirm the snapshot and 48-byte
   frames, both halves' positions, complete layers, home-row modifier holds,
   endpoint/profile, indicators, and both available batteries.
5. Run `kb-hud`; confirm a held Shift modifier is distinguished from its tap
   key and `/` previews as `?`, while normal HID typing remains unaffected.
6. Confirm the split-status UI remains hidden because its validity bit is
   intentionally clear in this firmware revision.

The builds retain the two known upstream warnings: deprecated
`NRF_STORE_REBOOT_TYPE_GPREGRET` on both halves, and the right build resolving
the central-only requested `ZMK_USB=y` to `n`. No telemetry compile warning or
unknown devicetree-property warning was emitted.
