{
  lib,
  stdenv,
  fetchFromGitHub,
  autoreconfHook,
  sdcc,
}:
stdenv.mkDerivation (finalAttrs: {
  pname = "sigrok-firmware-fx2lafw";
  version = "0.1.7-unstable-2024-02-04";

  src = fetchFromGitHub {
    owner = "sigrokproject";
    repo = "sigrok-firmware-fx2lafw";
    rev = "0f2d3242ffb5582e5b9a018ed9ae9812d517a56e";
    hash = "sha256-xveVcwAwtqKGD3/UvnBz5ASvTyg/6jAlTedZElhV2HE=";
  };

  nativeBuildInputs = [
    autoreconfHook
    sdcc
  ];

  enableParallelBuilding = true;
})
