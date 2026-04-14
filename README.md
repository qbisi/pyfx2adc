# pyfx2adc

`pyfx2adc` provides Python bindings for `libfx2adc` using `pybind11`.

## Requirements

- Python 3.10+
- `pybind11`
- `scikit-build-core`
- An already installed `libfx2adc`

The package does not vendor `fx2adc` and does not build the C library for you.
At build time, it looks for `libfx2adc` using:

1. `find_package(fx2adc CONFIG)`
2. `pkg-config` for `libfx2adc`

Firmware files, USB permissions, and udev rules still come from the main
`fx2adc` installation.

## Build

Typical editable install:

```bash
python3 -m pip install -e .
```

If `libfx2adc` is installed in a non-standard location, point CMake or
`pkg-config` at it in the usual way, for example with `CMAKE_PREFIX_PATH` or
`PKG_CONFIG_PATH`.

## Usage

```python
import pyfx2adc

print(pyfx2adc.device_count())
dev = pyfx2adc.open(0)
print(dev.get_usb_strings())

def on_samples(chunk: bytes) -> None:
    print(len(chunk))
    dev.cancel_async()

dev.read(on_samples)
dev.close()
```

`Device.read()` is blocking and follows the same model as the C API. It returns
only after streaming is cancelled, the device is lost, or an error occurs.

## Notes

- Sample chunks are delivered as Python `bytes` in v1.
- Callback exceptions stop streaming and are surfaced as `StreamingError`.
- The package links against GPL-licensed `libfx2adc`, so distribution must
  remain GPL-compatible.

