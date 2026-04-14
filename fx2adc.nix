{
  lib,
  stdenv,
  fetchFromGitHub,
  cmake,
  pkg-config,
  libusb1,
}:
stdenv.mkDerivation {
  pname = "fx2adc";
  version = "0-unstable-2025-02-27";

  src = fetchFromGitHub {
    owner = "steve-m";
    repo = "fx2adc";
    rev = "2fe61f2be5615b531770cf00e29ba6196e43a022";
    hash = "sha256-Gv8f5KMCE6Ws2mDK+Nt8sXiawuKYkoKWosxGLg1rECs=";
  };

  patches = [ ./nixos-firmware.patch ];

  nativeBuildInputs = [
    cmake
    pkg-config
  ];

  buildInputs = [
    libusb1
  ];

  cmakeFlags = [
    (lib.cmakeBool "INSTALL_UDEV_RULES" false)
    (lib.cmakeBool "INSTALL_FIRMWARE" false)
    (lib.cmakeBool "DETACH_KERNEL_DRIVER" true)
  ];
}
