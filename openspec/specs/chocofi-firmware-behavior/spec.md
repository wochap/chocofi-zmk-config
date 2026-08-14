# Chocofi Firmware Behavior Specification

## Purpose
Define the required hardware roles, physical layout, layers, key behaviors, combos, and power behavior for the Chocofi firmware.

## Requirements

### Requirement: Split hardware identity and roles
The firmware SHALL target a 36-key Chocofi using two `nice_nano_v2` controllers and the official Corne split shield. The `corne_left` image SHALL be the split central, the `corne_right` image SHALL be the split peripheral, and the host-visible keyboard name SHALL be `Chocochap`.

#### Scenario: Build the central half
- **WHEN** the configuration is built for `nice_nano_v2` with shield `corne_left`
- **THEN** the generated configuration enables the split central role and host-facing USB and BLE support under the name `Chocochap`

#### Scenario: Build the peripheral half
- **WHEN** the configuration is built for `nice_nano_v2` with shield `corne_right`
- **THEN** the generated configuration enables split peripheral operation without making the right half the host-facing USB central

### Requirement: Native 36-key physical layout
The firmware SHALL select ZMK's official `foostan_corne_5col_layout` and SHALL define exactly 36 bindings on each layer. Position groups SHALL distinguish the fifteen left finger keys, fifteen right finger keys, and six thumb keys for positional behavior.

#### Scenario: Compile each layer
- **WHEN** devicetree compiles the keymap
- **THEN** every layer has exactly 36 bindings matching the selected five-column Corne physical layout

#### Scenario: Draw the keymap
- **WHEN** keymap-drawer parses the keymap
- **THEN** it identifies the five-column Corne layout and produces a 36-key drawing without placeholder outer columns

### Requirement: Stable layer IDs
The keymap SHALL define Colemak-DH as layer 0, QWERTY as layer 1, NUM as layer 2, NAV as layer 3, FN as layer 4, and ADJUST as layer 5. Layer node order and all layer-targeting behaviors SHALL agree with these IDs.

#### Scenario: Resolve layer bindings
- **WHEN** the compiled keymap resolves a layer-targeting binding
- **THEN** its numeric target corresponds to the defined six-layer order

### Requirement: Colemak-DH layer bindings
The Colemak-DH layer SHALL preserve these physical bindings:

```text
Q W F P B | J L U Y ;
A R S T G | M N E I O
Z X C D V | K H , . /
LSHIFT  LT(NUM,TAB)  SPACE | BSPC  LT(NAV,ENTER)  RSHIFT
```

The first four left home-row keys SHALL hold left GUI, Alt, Control, and Shift respectively. The last four right home-row keys SHALL hold right Shift, Control, Alt, and GUI respectively.

#### Scenario: Type on the default layer
- **WHEN** no non-default layer is active and each finger or thumb position is tapped
- **THEN** it emits the Colemak-DH tap binding shown above

#### Scenario: Hold a Colemak-DH home-row modifier
- **WHEN** a Colemak-DH home-row mod resolves as a hold
- **THEN** it holds the modifier assigned to that physical position

### Requirement: QWERTY layer bindings
The QWERTY layer SHALL preserve these physical bindings:

```text
Q W E R T | Y U I O P
A S D F G | H J K L ;
Z X C V B | N M , . /
LSHIFT  LT(NUM,TAB)  SPACE | BSPC  LT(NAV,ENTER)  RSHIFT
```

The first four left home-row keys SHALL hold left GUI, Alt, Control, and Shift respectively. The last four right home-row keys SHALL hold right Shift, Control, Alt, and GUI respectively.

#### Scenario: Type with QWERTY enabled
- **WHEN** QWERTY is toggled on and each finger or thumb position is tapped
- **THEN** it emits the QWERTY tap binding shown above

#### Scenario: Return to Colemak-DH
- **WHEN** the QWERTY toggle on ADJUST is invoked while QWERTY is active
- **THEN** layer 1 is disabled and Colemak-DH again supplies the base bindings

### Requirement: NUM layer bindings
The NUM layer SHALL preserve these physical bindings, where `NONE` is non-transparent and `TRANS` falls through:

```text
8 7 3 0 5 | 6 2 1 9 4
! $ ( ) @ | \ - = * ^
NONE NONE # ' ` | & [ ] % NONE
TRANS  TRANS  NONE | DEL  LT(NAV,ESC)  TRANS
```

The first four left home-row symbols SHALL hold left GUI, Alt, Control, and Shift. The final four right home-row symbols (`-`, `=`, `*`, `^`) SHALL hold right Shift, Control, Alt, and GUI.

#### Scenario: Use the number layer
- **WHEN** NUM is active and a NUM position is tapped
- **THEN** it emits the NUM binding or transparency shown above

### Requirement: NAV layer bindings
The NAV layer SHALL preserve these physical bindings:

```text
F1 F2 F3 F4 F5 | F6 F7 F8 F9 F10
F11 HOME PG_UP PG_DN END | LEFT DOWN UP RIGHT F12
NONE NONE NONE NONE CAPS | PSCRN NONE NONE NONE SL(ADJUST)
TRANS  NUM-switch-macro  MO(FN) | NONE  TRANS  TRANS
```

The first four left home-row bindings SHALL hold left GUI, Alt, Control, and Shift. The final four right home-row bindings (`DOWN`, `UP`, `RIGHT`, `F12`) SHALL hold right Shift, Control, Alt, and GUI.

#### Scenario: Navigate and access FN
- **WHEN** NAV is active
- **THEN** navigation, function-key, sticky ADJUST, NUM-switch, and momentary FN positions behave as shown above

### Requirement: FN layer bindings
The FN layer SHALL preserve play/pause on the third right top-row position; brightness up and down on the third and fourth left home positions; previous track, volume down, volume up, next track, and mute across the right home row; transparent outer thumb positions as currently defined; and non-transparent unused positions elsewhere.

#### Scenario: Use media controls
- **WHEN** FN is active and a defined media or brightness position is tapped
- **THEN** it emits the existing consumer-control binding assigned to that position

### Requirement: ADJUST layer controls
The ADJUST layer SHALL preserve USB, BLE, and output-toggle controls; external-power toggle, on, and off controls; Bluetooth profile selection for profiles 0 through 4; clear-all and clear-current Bluetooth controls; mirrored bootloader and system-reset controls; and the QWERTY layer toggle at the existing physical positions. All other ADJUST positions SHALL remain non-transparent.

#### Scenario: Select a Bluetooth profile
- **WHEN** ADJUST is active and profile position 0 through 4 is pressed
- **THEN** ZMK selects the corresponding Bluetooth profile

#### Scenario: Select an output
- **WHEN** ADJUST is active and an output-selection position is pressed
- **THEN** ZMK selects USB, selects BLE, or toggles the output according to that position

#### Scenario: Invoke maintenance controls
- **WHEN** ADJUST is active and a bootloader, reset, external-power, or QWERTY-toggle position is pressed
- **THEN** the corresponding official ZMK behavior is invoked

### Requirement: Official home-row hold-tap semantics
The left and right home-row behaviors SHALL use official `zmk,behavior-hold-tap` nodes with `flavor = "balanced"`, `tapping-term-ms = <275>`, `quick-tap-ms = <150>`, `require-prior-idle-ms = <150>`, and `hold-trigger-on-release`. A left-hand home-row hold SHALL be positionally triggered only by right-hand finger positions or thumbs, and a right-hand home-row hold SHALL be positionally triggered only by left-hand finger positions or thumbs.

#### Scenario: Cross-hand chord
- **WHEN** a home-row mod is held and a trigger position on the opposite hand is used
- **THEN** the home-row behavior can resolve to its assigned modifier under balanced hold-tap semantics

#### Scenario: Same-hand roll
- **WHEN** another finger position on the same hand interrupts and is released within the tapping term
- **THEN** the positional behavior favors the home-row tap rather than treating that same-hand roll as a modifier chord

#### Scenario: Thumb chord
- **WHEN** a thumb position participates while a home-row mod is undecided
- **THEN** the thumb position is eligible to trigger the home-row hold on either hand

#### Scenario: Verify compiled properties
- **WHEN** the generated devicetree is inspected after a successful build
- **THEN** every required home-row property and exact timing value is present on both hold-tap nodes

### Requirement: Layer-tap semantics
The custom layer-tap SHALL use `flavor = "hold-preferred"`, `tapping-term-ms = <125>`, and `quick-tap-ms = <150>`, with `&mo` as the hold behavior and `&kp` as the tap behavior. It SHALL implement Tab/NUM and Enter/NAV on both base layers and Escape/NAV on NUM.

#### Scenario: Tap a layer thumb
- **WHEN** a custom layer-tap is released as a tap
- **THEN** it emits Tab, Enter, or Escape according to its position and active layer

#### Scenario: Hold a layer thumb
- **WHEN** a custom layer-tap resolves as a hold
- **THEN** it activates NUM or NAV only for the duration of the hold

### Requirement: NAV-to-NUM transition macro
The existing zero-parameter macro SHALL release the momentary NAV behavior, press momentary NUM, pause until the macro key is released, and then release momentary NUM, using a one-millisecond macro wait.

#### Scenario: Switch from held NAV to NUM
- **WHEN** NAV is active through the thumb hold and the NAV-layer NUM-switch macro is pressed
- **THEN** NAV is released and NUM remains active only while the macro key is held

### Requirement: Editing combos
The keymap SHALL define the following combos on Colemak-DH and QWERTY only: physical X+D emits Control+X, X+C emits Control+Insert, C+D emits Shift+Insert, and W+F emits Control+A. Every combo SHALL use `timeout-ms = <18>` and `require-prior-idle-ms = <150>`.

#### Scenario: Trigger an editing combo on a base layer
- **WHEN** one of the defined physical key pairs is pressed within 18 milliseconds after at least 150 milliseconds of prior idle on Colemak-DH or QWERTY
- **THEN** it emits the corresponding cut, copy, paste, or select-all binding

#### Scenario: Press a combo pair on another layer
- **WHEN** the same positions are pressed while NUM, NAV, FN, or ADJUST supplies the active bindings
- **THEN** the base-layer combo is not enabled for that layer

### Requirement: Power configuration
Both halves SHALL enable deep sleep after 1,800,000 milliseconds of inactivity, +8 dBm Bluetooth controller transmit power, and ZMK's experimental BLE connection configuration.

#### Scenario: Inspect generated half configuration
- **WHEN** either half is built
- **THEN** its generated Kconfig contains the required sleep timeout, transmit power, and BLE connection settings
