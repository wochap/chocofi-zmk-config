{
  description = "Reproducible local build environment for Chocofi ZMK firmware";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?rev=0ad6f47ea4fe188f4bc8f0380f93ae8523337c6c"; # nixos-26.05 (10 jul 2026)

    # Zephyr v3.5.0+zmk-fixes, matching config/west.yml.
    zephyr.url = "github:zmkfirmware/zephyr?rev=dacab4875df72109b96cc8977547a0dc04875bcd";
    zephyr.flake = false;

    # Provides the Zephyr Python environment and SDK 0.16.8 packaging.
    zephyr-nix.url = "github:nix-community/zephyr-nix?rev=a12131ec450ea66e9005c668c31c8a055a766ef3";
    zephyr-nix.inputs.zephyr.follows = "zephyr";
  };

  outputs = { nixpkgs, zephyr-nix, ... }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
    in {
      devShells = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          zephyr = zephyr-nix.packages.${system};
        in {
          default = pkgs.mkShellNoCC {
            packages = [
              zephyr.pythonEnv
              (zephyr.sdk.override { targets = [ "arm-zephyr-eabi" ]; })

              pkgs.cmake
              pkgs.coreutils
              pkgs.dtc
              pkgs.findutils
              pkgs.git
              pkgs.gnugrep
              pkgs.gnused
              pkgs.ninja

              pkgs.just
              pkgs.keymap-drawer
            ];
          };
        });
    };
}
