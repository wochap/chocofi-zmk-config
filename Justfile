set shell := ["bash", "-eu", "-o", "pipefail", "-c"]

default:
    @just --list --unsorted

# Initialize this directory as a west workspace and resolve every pinned project.
init:
    if [[ ! -f .west/config ]]; then west init -l config; fi
    west update --narrow --fetch-opt=--filter=blob:none
    west zephyr-export

# Re-resolve the immutable manifest without changing its declared revisions.
update:
    west update --narrow --fetch-opt=--filter=blob:none
    west zephyr-export

# Build the central (left) half and copy its UF2 into firmware/.
build-left:
    just _build left

# Build the peripheral (right) half and copy its UF2 into firmware/.
build-right:
    just _build right

# Build both halves.
build-all: build-left build-right

# Generate keymap-drawer's parsed YAML and six-layer SVG.
draw:
    mkdir -p draw
    keymap parse -z config/corne.keymap -o draw/corne.yaml
    keymap draw draw/corne.yaml -d zmk/app/dts/layouts/foostan/corne/5column.dtsi -l foostan_corne_5col_layout -o draw/corne.svg

# Remove generated build and firmware outputs.
clean:
    rm -rf -- .build firmware

[private]
_build half:
    mkdir -p firmware
    west build -p always -s zmk/app -d ".build/corne_{{ half }}-nice_nano_v2" -b nice_nano_v2 -- -DSHIELD="corne_{{ half }}" -DZMK_CONFIG="${PWD}/config"
    install -Dm644 ".build/corne_{{ half }}-nice_nano_v2/zephyr/zmk.uf2" "firmware/corne_{{ half }}-nice_nano_v2.uf2"
