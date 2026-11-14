#include "vrc5477_io_pci_function.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/state/state_stream.h>
#include "vrc5477_pci_channel.h"

namespace cerfcore {

namespace {

constexpr uint16_t kCommandWritable = 0xFFFFu;
constexpr uint8_t  kByteWritable    = 0xFFu;

}

Vrc5477IoPciFunction::Vrc5477IoPciFunction(CerfEmulator& emu, uint8_t dev, uint32_t identity, uint32_t class_rev,
                                           PciBar bar0)
    : emu_(emu),
      dev_(dev),
      identity_{{identity, class_rev, 0u, 0u, kCommandWritable, kByteWritable, kByteWritable, 0u}, 0u, kByteWritable, 0u},
      header_(identity_) {
    header_.SetBarReset(0, bar0);
}

void Vrc5477IoPciFunction::Reset() { header_.Reset(); }

uint32_t Vrc5477IoPciFunction::Line() const { return Vrc5477PciChannel::IdselLineOfDevice(dev_); }

bool Vrc5477IoPciFunction::HeaderStub(uint32_t reg) {
    return reg == PciType0Header::kRegCardbusCis || reg == PciType0Header::kRegSubsystem ||
           (reg >= PciType0Header::kRegExpansionRom && reg < PciType0Header::kRegInterrupt);
}

uint32_t Vrc5477IoPciFunction::ConfigRead(uint32_t reg) {
    uint32_t value = 0u;
    if (header_.Read(reg, &value)) return value;
    if (HeaderStub(reg) || reg >= PciType0Header::kRegDeviceFirst) return 0u;
    emu_.Get<Fatal>().Die("VRC5477 internal PCI AD%u: config read reg=0x%02X", Line(), reg);
}

void Vrc5477IoPciFunction::ConfigWrite(uint32_t reg, uint32_t value) {
    if (header_.Write(reg, value) || HeaderStub(reg)) return;
    emu_.Get<Fatal>().Die("VRC5477 internal PCI AD%u: config write reg=0x%02X val=0x%08X", Line(), reg, value);
}

bool Vrc5477IoPciFunction::MemClaims(uint32_t pci_addr) const { return header_.Decodes(pci_addr, false); }

uint32_t Vrc5477IoPciFunction::MemRead(uint32_t pci_addr, unsigned size) {
    emu_.Get<Fatal>().Die("VRC5477 internal PCI AD%u: memory read pci=0x%08X size=%u", Line(), pci_addr, size);
}

void Vrc5477IoPciFunction::MemWrite(uint32_t pci_addr, uint32_t value, unsigned size) {
    emu_.Get<Fatal>().Die("VRC5477 internal PCI AD%u: memory write pci=0x%08X val=0x%08X size=%u", Line(),
                          pci_addr, value, size);
}

bool Vrc5477IoPciFunction::IoClaims(uint32_t pci_io) const { return header_.Decodes(pci_io, true); }

uint32_t Vrc5477IoPciFunction::IoRead(uint32_t pci_io, unsigned size) {
    emu_.Get<Fatal>().Die("VRC5477 internal PCI AD%u: I/O read pci=0x%08X size=%u", Line(), pci_io, size);
}

void Vrc5477IoPciFunction::IoWrite(uint32_t pci_io, uint32_t value, unsigned size) {
    emu_.Get<Fatal>().Die("VRC5477 internal PCI AD%u: I/O write pci=0x%08X val=0x%08X size=%u", Line(), pci_io,
                          value, size);
}

void Vrc5477IoPciFunction::SaveState(StateWriter& w) {
    StateWriteField field(w);
    PciType0Header::Visit(header_, field);
}

void Vrc5477IoPciFunction::RestoreState(StateReader& r) {
    StateReadField field(r);
    PciType0Header::Visit(header_, field);
}

}
