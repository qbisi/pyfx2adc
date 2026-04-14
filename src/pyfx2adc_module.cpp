#include <atomic>
#include <cerrno>
#include <mutex>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

#include <libusb.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <fx2adc.h>

namespace py = pybind11;

namespace {

class Fx2AdcException : public std::runtime_error {
public:
    explicit Fx2AdcException(const std::string &message)
        : std::runtime_error(message) {}
};

class DeviceNotFoundException : public Fx2AdcException {
public:
    explicit DeviceNotFoundException(const std::string &message)
        : Fx2AdcException(message) {}
};

class AccessException : public Fx2AdcException {
public:
    explicit AccessException(const std::string &message)
        : Fx2AdcException(message) {}
};

class StreamingException : public Fx2AdcException {
public:
    explicit StreamingException(const std::string &message)
        : Fx2AdcException(message) {}
};

std::string usb_error_message(const char *action, int code)
{
    return std::string(action) + " failed with error " + std::to_string(code);
}

[[noreturn]] void throw_runtime_code(const char *action, int code)
{
    if (code == -ENOMEM || code == LIBUSB_ERROR_NO_MEM) {
        throw Fx2AdcException(std::string(action) + " failed: out of memory");
    }
    if (code == LIBUSB_ERROR_ACCESS) {
        throw AccessException(std::string(action) + " failed: access denied");
    }
    if (code == LIBUSB_ERROR_NOT_FOUND || code == LIBUSB_ERROR_NO_DEVICE) {
        throw DeviceNotFoundException(std::string(action) + " failed: device not found");
    }
    if (code == -2) {
        throw DeviceNotFoundException(std::string(action) + " failed: no matching device found");
    }
    throw Fx2AdcException(usb_error_message(action, code));
}

[[noreturn]] void throw_lookup_code(const char *action, int code)
{
    if (code == -1) {
        throw Fx2AdcException(std::string(action) + " failed: invalid input");
    }
    if (code == -2) {
        throw DeviceNotFoundException(std::string(action) + " failed: no devices were found");
    }
    if (code == -3) {
        throw DeviceNotFoundException(std::string(action) + " failed: no device matched the requested serial");
    }
    throw_runtime_code(action, code);
}

void throw_runtime_if_negative(const char *action, int code)
{
    if (code < 0) {
        throw_runtime_code(action, code);
    }
}

void throw_lookup_if_negative(const char *action, int code)
{
    if (code < 0) {
        throw_lookup_code(action, code);
    }
}

std::tuple<std::string, std::string, std::string> usb_strings_from_index(uint32_t index)
{
    char manufact[256] = {};
    char product[256] = {};
    char serial[256] = {};

    int rc = fx2adc_get_device_usb_strings(index, manufact, product, serial);
    throw_runtime_if_negative("fx2adc_get_device_usb_strings", rc);

    return std::make_tuple(std::string(manufact), std::string(product), std::string(serial));
}

class Device {
public:
    explicit Device(uint32_t index)
    {
        fx2adc_dev_t *raw = nullptr;
        int rc = fx2adc_open(&raw, index);
        throw_runtime_if_negative("fx2adc_open", rc);
        dev_ = raw;
    }

    Device(const Device &) = delete;
    Device &operator=(const Device &) = delete;

    Device(Device &&other) noexcept
    {
        std::scoped_lock lock(other.state_mutex_);
        dev_ = other.dev_;
        reading_.store(other.reading_.load());
        other.dev_ = nullptr;
        other.reading_.store(false);
    }

    Device &operator=(Device &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        close_noexcept();

        std::scoped_lock lock(state_mutex_, other.state_mutex_);
        dev_ = other.dev_;
        reading_.store(other.reading_.load());
        other.dev_ = nullptr;
        other.reading_.store(false);
        return *this;
    }

    ~Device()
    {
        close_noexcept();
    }

    void close()
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        ensure_open_unlocked();

        if (reading_.load()) {
            throw StreamingException("cannot close device while a read is active");
        }

        close_unlocked();
    }

    std::tuple<std::string, std::string, std::string> get_usb_strings()
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        ensure_open_unlocked();

        char manufact[256] = {};
        char product[256] = {};
        char serial[256] = {};

        int rc = fx2adc_get_usb_strings(dev_, manufact, product, serial);
        throw_runtime_if_negative("fx2adc_get_usb_strings", rc);

        return std::make_tuple(std::string(manufact), std::string(product), std::string(serial));
    }

    void set_vdiv(int channel, int millivolts)
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        ensure_open_unlocked();
        int rc = fx2adc_set_vdiv(dev_, channel, millivolts);
        throw_runtime_if_negative("fx2adc_set_vdiv", rc);
    }

    int get_vdiv()
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        ensure_open_unlocked();
        return fx2adc_get_vdiv(dev_);
    }

    void set_sample_rate(uint32_t rate, bool ext_clock = false)
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        ensure_open_unlocked();
        int rc = fx2adc_set_sample_rate(dev_, rate, ext_clock);
        throw_runtime_if_negative("fx2adc_set_sample_rate", rc);
    }

    uint32_t get_sample_rate()
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        ensure_open_unlocked();
        return fx2adc_get_sample_rate(dev_);
    }

    void cancel_async()
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        ensure_open_unlocked();

        int rc = fx2adc_cancel_async(dev_);
        if (rc < 0 && rc != -2) {
            throw StreamingException(usb_error_message("fx2adc_cancel_async", rc));
        }
    }

    void read(py::object callback, uint32_t buf_num = 0, uint32_t buf_len = 0)
    {
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            ensure_open_unlocked();
            if (reading_.exchange(true)) {
                throw StreamingException("read is already active on this device");
            }
        }

        if (!PyCallable_Check(callback.ptr())) {
            reading_.store(false);
            throw py::type_error("callback must be callable");
        }

        CallbackState cb_state{this, std::move(callback), false, ""};
        int rc = 0;

        try {
            py::gil_scoped_release release;
            rc = fx2adc_read(dev_, &Device::read_callback, &cb_state, buf_num, buf_len);
        } catch (...) {
            reading_.store(false);
            throw;
        }

        reading_.store(false);

        if (cb_state.python_error) {
            throw StreamingException(cb_state.python_error_message);
        }

        throw_runtime_if_negative("fx2adc_read", rc);
    }

private:
    struct CallbackState {
        Device *owner;
        py::object callback;
        bool python_error;
        std::string python_error_message;
    };

    static void read_callback(unsigned char *buf, uint32_t len, void *ctx)
    {
        auto *state = static_cast<CallbackState *>(ctx);
        py::gil_scoped_acquire acquire;

        try {
            py::bytes chunk(reinterpret_cast<const char *>(buf), static_cast<py::ssize_t>(len));
            state->callback(chunk);
        } catch (py::error_already_set &exc) {
            state->python_error = true;
            state->python_error_message = exc.what();
            state->owner->request_cancel_from_callback();
        } catch (const std::exception &exc) {
            state->python_error = true;
            state->python_error_message = exc.what();
            state->owner->request_cancel_from_callback();
        }
    }

    void request_cancel_from_callback() noexcept
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (dev_ != nullptr) {
            (void)fx2adc_cancel_async(dev_);
        }
    }

    void ensure_open_unlocked() const
    {
        if (dev_ == nullptr) {
            throw Fx2AdcException("device is already closed");
        }
    }

    void close_unlocked()
    {
        fx2adc_dev_t *to_close = dev_;
        dev_ = nullptr;
        reading_.store(false);

        int rc = fx2adc_close(to_close);
        throw_runtime_if_negative("fx2adc_close", rc);
    }

    void close_noexcept() noexcept
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (dev_ == nullptr || reading_.load()) {
            return;
        }

        fx2adc_dev_t *to_close = dev_;
        dev_ = nullptr;
        reading_.store(false);
        (void)fx2adc_close(to_close);
    }

    fx2adc_dev_t *dev_ = nullptr;
    std::mutex state_mutex_;
    std::atomic<bool> reading_{false};
};

Device open_device(uint32_t index)
{
    return Device(index);
}

} // namespace

PYBIND11_MODULE(_pyfx2adc, m)
{
    m.doc() = "Python bindings for libfx2adc";

    auto exc_base = py::register_exception<Fx2AdcException>(m, "Fx2AdcError");
    py::register_exception<DeviceNotFoundException>(m, "DeviceNotFoundError", exc_base.ptr());
    py::register_exception<AccessException>(m, "AccessError", exc_base.ptr());
    py::register_exception<StreamingException>(m, "StreamingError", exc_base.ptr());

    m.def("device_count", []() {
        return fx2adc_get_device_count();
    }, "Return the number of supported devices currently visible.");

    m.def("device_name", [](uint32_t index) {
        return std::string(fx2adc_get_device_name(index));
    }, py::arg("index"), "Return the model name for a device index.");

    m.def("device_usb_strings", &usb_strings_from_index, py::arg("index"),
          "Return manufacturer, product, and serial strings for a device index.");

    m.def("index_by_serial", [](const std::string &serial) {
        int rc = fx2adc_get_index_by_serial(serial.c_str());
        throw_lookup_if_negative("fx2adc_get_index_by_serial", rc);
        return rc;
    }, py::arg("serial"), "Return the first device index matching the USB serial.");

    py::class_<Device>(m, "Device")
        .def("close", &Device::close, "Close the underlying device handle.")
        .def("get_usb_strings", &Device::get_usb_strings,
             "Return manufacturer, product, and serial strings.")
        .def("set_vdiv", &Device::set_vdiv, py::arg("channel"), py::arg("millivolts"),
             "Set the voltage divider in millivolts per division.")
        .def("get_vdiv", &Device::get_vdiv, "Return the configured voltage divider in millivolts.")
        .def("set_sample_rate", &Device::set_sample_rate, py::arg("rate"), py::arg("ext_clock") = false,
             "Set the device sample rate in samples per second.")
        .def("get_sample_rate", &Device::get_sample_rate, "Return the configured sample rate.")
        .def("read", &Device::read, py::arg("callback"), py::arg("buf_num") = 0, py::arg("buf_len") = 0,
             "Read samples synchronously and deliver each chunk to a Python callback.")
        .def("cancel_async", &Device::cancel_async, "Cancel an active read operation.");

    m.def("open", &open_device, py::arg("index"), "Open a device by index.");
}
