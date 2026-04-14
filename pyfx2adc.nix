{
  lib,
  buildPythonPackage,
  ninja,
  cmake,
  pkg-config,
  scikit-build-core,
  pybind11,
  fx2adc,
  pytestCheckHook,
}:
buildPythonPackage {
  pname = "pyfx2adc";
  version = "0.1.0";
  pyproject = true;
  src = ./.;

  build-system = [
    scikit-build-core
    pybind11
  ];

  nativeBuildInputs = [
    cmake
    ninja
    pkg-config
  ];

  buildInputs = [
    fx2adc
  ];

  dontUseCmakeConfigure = true;

  cmakeFlags = [
    (lib.cmakeBool "CMAKE_INSTALL_RPATH_USE_LINK_PATH" true)
  ];

  preCheck = ''
    cd tests
  '';

  nativeCheckInputs = [
    pytestCheckHook
  ];

  pythonImportsCheck = [
    "pyfx2adc"
  ];
}
