#include "vrc5477_io_pci_function.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/state/state_stream.h>
#include "vrc5477_pci_channel.h"

namespace cerfcore {

namespace {

constexpr uint32_t kRegCommand      = 0x04u;
constexpr uint32_t kRegCacheLatency = 0x0Cu;
constexpr uint32_t kRegBarFirst     = 0x10u;
constexpr uint32_t kRegBarLast      = 0x24u;
constexpr uint32_t kRegCardbusCis   = 0x28u;
constexpr uint32_t kRegSubsystem    = 0x2Cu;
constexpr uint32_t kRegInterrupt    = 0x3Cu;

constexpr uint32_t kCommandIoSpace     = 0x1u;
constexpr uint32_t kCommandMemorySpace = 0x2u;

}

Vrc5477IoPciFunction::Vrc5477IoPciFunction(CerfEmulator& emu, uint8_t dev, PciBar bar0)
    : emu_(emu), dev_(dev) {
    bars_[0] = bar0;
}

void Vrc5477IoPciFunction::Reset() {
    for (PciBar& b : bars_) b.base = 0u;
    command_       = 0u;
    cache_latency_ = 0u;
    interrupt_     = 0u;
}

uint32_t Vrc5477IoPciFunction::Line() const { return Vrc5477PciChannel::IdselLineOfDevice(dev_); }

uint32_t Vrc5477IoPciFunction::ConfigRead(uint32_t reg) {
    if (reg >= kRegBarFirst && reg <= kRegBarLast) return bars_[(reg - kRegBarFirst) / 4u].Read();
    if (reg == kRegCardbusCis || reg == kRegSubsystem) return 0u;
    emu_.Get<Fatal>().Die("VRC5477 internal PCI AD%u: config read reg=0x%02X", Line(), reg);
}

void Vrc5477IoPciFunction::ConfigWrite(uint32_t reg, uint32_t value) {
    if (reg >= kRegBarFirst && reg <= kRegBarLast) {
        bars_[(reg - kRegBarFirst) / 4u].Write(value);
    } else if (reg == kRegCommand) {
        command_ = value;
    } else if (reg == kRegCacheLatency) {
        cache_latency_ = value;
    } else if (reg == kRegInterrupt) {
        interrupt_ = value;
    } else if (reg != kRegCardbusCis && reg != kRegSubsystem) {
        emu_.Get<Fatal>().Die("VRC5477 internal PCI AD%u: config write reg=0x%02X val=0x%08X", Line(), reg,
                              value);
    }
}

bool Vrc5477IoPciFunction::MemClaims(uint32_t pci_addr) const {
    if ((command_ & kCommandMemorySpace) == 0u) return false;
    for (const PciBar& b : bars_)
        if (!b.IsIo() && b.Decodes(pci_addr)) return true;
    return false;
}

uint32_t Vrc5477IoPciFunction::MemRead(uint32_t pci_addr, unsigned size) {
    emu_.Get<Fatal>().Die("VRC5477 internal PCI AD%u: memory read pci=0x%08X size=%u", Line(), pci_addr, size);
}

void Vrc5477IoPciFunction::MemWrite(uint32_t pci_addr, uint32_t value, unsigned size) {
    emu_.Get<Fatal>().Die("VRC5477 internal PCI AD%u: memory write pci=0x%08X val=0x%08X size=%u", Line(),
                          pci_addr, value, size);
}

bool Vrc5477IoPciFunction::IoClaims(uint32_t pci_io) const {
    if ((command_ & kCommandIoSpace) == 0u) return false;
    for (const PciBar& b : bars_)
        if (b.IsIo() && b.Decodes(pci_io)) return true;
    return false;
}

uint32_t Vrc5477IoPciFunction::IoRead(uint32_t pci_io, unsigned size) {
    emu_.Get<Fatal>().Die("VRC5477 internal PCI AD%u: I/O read pci=0x%08X size=%u", Line(), pci_io, size);
}

void Vrc5477IoPciFunction::IoWrite(uint32_t pci_io, uint32_t value, unsigned size) {
    emu_.Get<Fatal>().Die("VRC5477 internal PCI AD%u: I/O write pci=0x%08X val=0x%08X size=%u", Line(), pci_io,
                          value, size);
}

void Vrc5477IoPciFunction::SaveState(StateWriter& w) {
    StateWriteField bar_field(w);
    for (PciBar& b : bars_) PciBar::Visit(b, bar_field);
    w.Write("command", command_);
    w.Write("cache_latency", cache_latency_);
    w.Write("interrupt", interrupt_);
}

void Vrc5477IoPciFunction::RestoreState(StateReader& r) {
    StateReadField bar_field(r);
    for (PciBar& b : bars_) PciBar::Visit(b, bar_field);
    r.Read("command", command_);
    r.Read("cache_latency", cache_latency_);
    r.Read("interrupt", interrupt_);
}

}
