#pragma once

#include <cerfcore/core/callback_list.h>

#include <cstdint>
#include <functional>
#include <vector>

namespace cerfcore {

class CerfEmulator;
class PciDevice;
class StateReader;
class StateWriter;

class Vrc5477PciChannel {
public:
    enum class Register { kWindow0, kInitiator0, kWindow1, kInitiator1 };

    static constexpr uint32_t kFirstIdselLine = 11u;

    static constexpr uint8_t DeviceOfIdselLine(uint32_t line) {
        return static_cast<uint8_t>(line - kFirstIdselLine + 1u);
    }
    static constexpr uint32_t IdselLineOfDevice(uint8_t dev) { return dev + kFirstIdselLine - 1u; }

    Vrc5477PciChannel(CerfEmulator& emu, const char* name, uint32_t window_base, uint32_t window_size,
                      uint32_t io_window_base, uint32_t io_window_size);

    uint32_t WindowBase() const { return window_base_; }
    uint32_t WindowSize() const { return window_size_; }
    uint32_t IoWindowBase() const { return io_window_base_; }
    uint32_t IoWindowSize() const { return io_window_size_; }

    void RegisterDevice(PciDevice* dev);
    void RegisterBusResetListener(std::function<void()> listener);

    uint32_t ReadRegister(Register reg) const;
    void     WriteRegister(Register reg, uint32_t value);
    void     WriteUpperWord(Register reg, uint32_t value);

    void ResetControl();
    void SetResetLine(bool asserted);

    uint32_t Read(uint32_t pa, unsigned size);
    void     Write(uint32_t pa, uint32_t value, unsigned size);
    uint64_t ReadDword(uint32_t pa);
    void     WriteDword(uint32_t pa, uint64_t value);
    uint32_t IoRead(uint32_t pa, unsigned size);
    void     IoWrite(uint32_t pa, uint32_t value, unsigned size);

    void SaveState(StateWriter& w);
    void RestoreState(StateReader& r);
    void PostRestore();

private:
    enum class Cycle { kMemory, kConfigType0, kConfigType1 };

    uint32_t   WindowOffset(uint32_t pdar, uint32_t pa, uint32_t* size);
    uint32_t   PciAddress(uint32_t pa, bool wide, Cycle* cycle);
    uint32_t   PciIoAddress(uint32_t pa);
    PciDevice* ConfigDevice(uint32_t pci, uint32_t* reg);

    [[noreturn]] void Reject(const char* what, uint32_t addr, uint32_t value);

    CerfEmulator&           emu_;
    const char*             name_;
    uint32_t                window_base_;
    uint32_t                window_size_;
    uint32_t                io_window_base_;
    uint32_t                io_window_size_;
    std::vector<PciDevice*> devices_;
    CallbackList<>          bus_reset_;
    uint32_t                regs_[4]   = {};
    bool                    reset_held_ = false;
};

}
