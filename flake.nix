{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.05";

    # Version of requirements.txt installed in pythonEnv
    zephyr.url = "github:zephyrproject-rtos/zephyr/v3.5.0";
    zephyr.flake = false;

    # Zephyr sdk and toolchain
    zephyr-nix.url = "github:urob/zephyr-nix";
    zephyr-nix.inputs.zephyr.follows = "zephyr";

    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { nixpkgs, zephyr-nix, flake-utils, ... }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        zephyr = zephyr-nix.packages.${system};
      in {
        devShells.default = pkgs.mkShell {
          packages = [
            zephyr.pythonEnv
            (zephyr.sdk.override { targets = [ "arm-zephyr-eabi" ]; })

            pkgs.cmake
            pkgs.dtc
            pkgs.ninja
            pkgs.qemu # needed for native_posix target
          ];
        };
      });
}

