import importlib
import unittest


class SmokeTests(unittest.TestCase):
    def test_import_and_symbols(self) -> None:
        module = importlib.import_module("pyfx2adc")

        self.assertTrue(hasattr(module, "Device"))
        self.assertTrue(hasattr(module, "device_count"))
        self.assertTrue(hasattr(module, "device_name"))
        self.assertTrue(hasattr(module, "device_usb_strings"))
        self.assertTrue(hasattr(module, "index_by_serial"))
        self.assertTrue(hasattr(module, "open"))
        self.assertTrue(hasattr(module, "Fx2AdcError"))
        self.assertTrue(hasattr(module, "DeviceNotFoundError"))
        self.assertTrue(hasattr(module, "AccessError"))
        self.assertTrue(hasattr(module, "StreamingError"))


if __name__ == "__main__":
    unittest.main()
