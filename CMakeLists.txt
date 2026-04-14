cmake_minimum_required(VERSION 3.18)

project(pyfx2adc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Python3 COMPONENTS Interpreter Development.Module REQUIRED)
find_package(pybind11 CONFIG REQUIRED)

set(FX2ADC_TARGET "")

find_package(fx2adc CONFIG QUIET)
if(TARGET fx2adc::fx2adc)
    set(FX2ADC_TARGET fx2adc::fx2adc)
endif()

if(NOT FX2ADC_TARGET)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        include(FindPkgConfig)
        pkg_check_modules(FX2ADC QUIET IMPORTED_TARGET libfx2adc)
        if(TARGET PkgConfig::FX2ADC)
            set(FX2ADC_TARGET PkgConfig::FX2ADC)
        endif()
    endif()
endif()

if(NOT FX2ADC_TARGET)
    message(FATAL_ERROR
        "Could not find libfx2adc. Install fx2adc first so it is discoverable via "
        "its CMake package config or pkg-config.")
endif()

pybind11_add_module(_pyfx2adc src/pyfx2adc_module.cpp)
target_link_libraries(_pyfx2adc PRIVATE ${FX2ADC_TARGET})
target_compile_definitions(_pyfx2adc PRIVATE PY_SSIZE_T_CLEAN)

install(TARGETS _pyfx2adc DESTINATION pyfx2adc)
