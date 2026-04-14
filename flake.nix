{
  description = "Nix flake for pyfx2adc";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = import nixpkgs {
          inherit system;
        };

        fx2adc = pkgs.stdenv.mkDerivation {
          pname = "fx2adc";
          version = "0-unstable-2025-02-27";

          src = pkgs.fetchFromGitHub {
            owner = "steve-m";
            repo = "fx2adc";
            rev = "2fe61f2be5615b531770cf00e29ba6196e43a022";
            hash = "sha256-Gv8f5KMCE6Ws2mDK+Nt8sXiawuKYkoKWosxGLg1rECs=";
          };

          nativeBuildInputs = with pkgs; [
            cmake
            pkg-config
          ];

          buildInputs = with pkgs; [
            libusb1
          ];

          cmakeFlags = [
            (pkgs.lib.cmakeBool "INSTALL_UDEV_RULES" false)
            (pkgs.lib.cmakeBool "INSTALL_FIRMWARE" false)
            (pkgs.lib.cmakeBool "DETACH_KERNEL_DRIVER" true)
          ];
        };

        pyfx2adc = pkgs.python3Packages.buildPythonPackage {
          pname = "pyfx2adc";
          version = "0.1.0";
          pyproject = true;
          src = ./.;

          nativeBuildInputs =
            with pkgs.python3Packages;
            [
              scikit-build-core
              pybind11
            ]
            ++ (with pkgs; [
              cmake
              ninja
              pkg-config
            ]);

          buildInputs = [
            fx2adc
          ];

          dontUseCmakeConfigure = true;

          cmakeFlags = [
            (pkgs.lib.cmakeBool "CMAKE_INSTALL_RPATH_USE_LINK_PATH" true)
          ];

          preCheck = ''
            cd tests
          '';

          nativeCheckInputs = with pkgs.python3Packages; [
            pytestCheckHook
          ];

          pythonImportsCheck = [
            "pyfx2adc"
          ];

          meta = with pkgs.lib; {
            description = "Python bindings for libfx2adc";
            homepage = "https://github.com/steve-m/fx2adc";
            license = licenses.gpl3Plus;
            platforms = platforms.linux;
          };
        };
      in
      {
        packages.default = pyfx2adc;
        packages.pyfx2adc = pyfx2adc;
        packages.fx2adc = fx2adc;
      }
    );
}
