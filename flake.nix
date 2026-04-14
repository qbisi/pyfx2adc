{
  description = "Nix flake for pyfx2adc";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    flake-compat.url = "github:nix-community/flake-compat";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
      ...
    }:
    {
      overlays.default = final: prev: {
        fx2adc = final.callPackage ./fx2adc.nix { };
        sigrok-firmware = final.callPackage ./sigrok-firmware.nix { };

        pythonPackagesExtensions = prev.pythonPackagesExtensions ++ [
          (python-final: python-prev: {
            pyfx2adc = python-final.callPackage ./pyfx2adc.nix { };
          })
        ];
      };
    }
    // flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = import nixpkgs {
          inherit system;
          overlays = [ self.overlays.default ];
        };
      in
      {
        packages.default = pkgs.python3Packages.pyfx2adc;
        packages.pyfx2adc = pkgs.python3Packages.pyfx2adc;
        packages.fx2adc = pkgs.fx2adc;
        packages.sigrok-firmware = pkgs.sigrok-firmware;
      }
    );
}
