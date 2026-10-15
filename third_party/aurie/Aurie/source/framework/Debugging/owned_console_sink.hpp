// Modified by nick on 14 October 2026 for Slormancer console fix revision 2.
// Changes and upstream attribution are described in docs/CREDITS.md of the runtime package.
#pragma once
#include <Windows.h>
#include <spdlog/sinks/wincolor_sink.h>
#include <memory>
#include <stdexcept>

namespace Aurie::Internal {
// Open a separate console handle instead of borrowing a CRT-owned stdout handle.
// freopen/_close and game stdout changes must not invalidate the logger's output.
class OwnedConsoleSink final : public spdlog::sinks::wincolor_sink<spdlog::details::console_mutex> {
    HANDLE ownedHandle;
public:
    explicit OwnedConsoleSink(HANDLE handle, spdlog::color_mode mode = spdlog::color_mode::automatic)
        : spdlog::sinks::wincolor_sink<spdlog::details::console_mutex>(handle, mode), ownedHandle(handle) {
        if (!handle || handle == INVALID_HANDLE_VALUE) throw std::invalid_argument("Invalid console output handle");
    }
    void ReleaseHandle() {
        std::lock_guard<spdlog::details::console_mutex::mutex_t> lock(this->mutex_);
        const HANDLE handle = ownedHandle;
        ownedHandle = nullptr;
        this->out_handle_ = nullptr;
        if (handle && handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
    }
    ~OwnedConsoleSink() override {ReleaseHandle();}
};

inline std::shared_ptr<OwnedConsoleSink> MakeOwnedConsoleSink() {
    const HANDLE handle = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return {};
    try {return std::make_shared<OwnedConsoleSink>(handle);}
    catch (...) {CloseHandle(handle); throw;}
}
}
