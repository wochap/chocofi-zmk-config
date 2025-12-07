## Chocofi ZMK config

### Layers

ColemakDH
![Imgur](https://i.imgur.com/1igKPoQ.jpg)

Querty
![Imgur](https://i.imgur.com/OMmmfWw.png)

Num
![Imgur](https://i.imgur.com/dsTCsc7.png)

Nav
![Imgur](https://i.imgur.com/uWaJWTx.png)

Adjust
![Imgur](https://i.imgur.com/0K5iIqP.png)

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

   ```sh
   $ west build -b proton_c -- -DSHIELD=kyria_left
   ```

   If your keymap is correct, you will see a progress in building until it fails; otherwise, it will simply fail outright

#### Build locally with nix

First time

```
$ cd ./path_to_this_repository
$ nix develop .#
$ just clean-all
$ just init
```

Then

```
$ cd ./path_to_this_repository
$ nix develop .#
$ just build all
$ # copy `firmware` folder content to `nice nano`
```
