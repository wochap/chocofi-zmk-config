{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?rev=0ad6f47ea4fe188f4bc8f0380f93ae8523337c6c"; # nixos-26.05 (10 jul 2026)

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

            pkgs.just
            pkgs.keymap-drawer
          ];
        };
      });
}
