## Chocofi zmk config

### Docs

ColemakDH layer
![Imgur](https://i.imgur.com/1igKPoQ.jpg)

Num layer
![Imgur](https://i.imgur.com/70txcxD.png)

Nav layer
![Imgur](https://i.imgur.com/dwhNjkD.png)

Adjust layer

> Images generated with http://www.keyboard-layout-editor.com/, use the files in `assets` folder

### Flashing

**IMPORTANT:** Always flash the **RIGHT** side first, then the left side

1. Download the latest firmware, in action tab
1. Enter bootloader mode, by pressing twice the reset button
1. Copy the corresponding side file to the `nice nano`

### Development

Git commit and push, GH Actions will build the firmware

#### Testing locally

1. Setup the [VSCode & Docker](https://zmk.dev/docs/development/setup) environment
1. Open zmk git repository in VSCode and enter into Dev Container
1. In VSCode terminal, run `cd app`
1. Edit `app/boards/shields/kyria/kyria_left.overlay` with your keymap and run

   ```
   $ west build -b proton_c -- -DSHIELD=kyria_left
   ```

   If your keymap is correct, you will see a progress in building until it fails; otherwise, it will simply fail outright
