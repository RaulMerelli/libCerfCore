#include "ali_m1535_internal_function.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/state/state_stream.h>

namespace cerfcore {

AliM1535InternalFunction::AliM1535InternalFunction(CerfEmulator& emu, const AliM1535BridgeConfig& bridge,
                                                   AliM1535Function function,
                                                   const AliM1535FunctionIdentity& identity,
                                                   AliM1535FunctionMemory* memory)
    : emu_(emu), bridge_(bridge), function_(function), identity_(identity), memory_(memory), header_(identity.header) {
    for (size_t i = 0; i < AliM1535FunctionIdentity::kBars; ++i) header_.SetBarReset(i, identity.bars[i]);
    Reset();
}

void AliM1535InternalFunction::Reset() {
    header_.Reset();
    pm_ = identity_.pm;
}

void AliM1535InternalFunction::Reject(const char* what, uint32_t addr, uint32_t value) {
    emu_.Get<Fatal>().Die("ALi M1535+ %s function AD%u is not modeled: %s addr=0x%08X val=0x%08X command=0x%04X",
                          identity_.name, identity_.idsel_line, what, addr, value, header_.Command());
}

uint32_t AliM1535InternalFunction::ConfigRead(uint32_t reg) {
    uint32_t value = 0u;
    if (header_.Read(reg, &value)) return value;
    if (reg == PciType0Header::kRegCapPtr) return pm_.offset;
    if (pm_.Owns(reg)) return pm_.Read(reg);
    return 0u;
}

void AliM1535InternalFunction::ConfigWrite(uint32_t reg, uint32_t value) {
    if (header_.Write(reg, value)) return;
    if (reg == PciType0Header::kRegSubsystem) {
        Reject("subsystem ID write", reg, value);
    } else if (pm_.Owns(reg)) {
        if (const char* why = pm_.WriteHoldingD0(reg, value)) Reject(why, reg, value);
    } else if (reg >= PciType0Header::kRegDeviceFirst) {
        Reject("device register write", reg, value);
    }
}

bool AliM1535InternalFunction::MemClaims(uint32_t pci_addr) const {
    return bridge_.Visible(function_) && header_.Decodes(pci_addr, false);
}

bool AliM1535InternalFunction::IoClaims(uint32_t pci_io) const {
    return bridge_.Visible(function_) && header_.Decodes(pci_io, true);
}

bool AliM1535InternalFunction::BusMasterEnabled() const {
    return (header_.Command() & PciCommonHeader::kCommandBusMaster) != 0u;
}

uint32_t AliM1535InternalFunction::MemRead(uint32_t pci_addr, unsigned size) {
    if (!memory_) Reject("memory read", pci_addr, size);
    return memory_->FunctionMemRead(pci_addr - header_.Bar(0).base, size);
}

void AliM1535InternalFunction::MemWrite(uint32_t pci_addr, uint32_t value, unsigned size) {
    if (!memory_) Reject("memory write", pci_addr, value);
    memory_->FunctionMemWrite(pci_addr - header_.Bar(0).base, value, size);
}

uint32_t AliM1535InternalFunction::IoRead(uint32_t pci_io, unsigned size) { Reject("I/O read", pci_io, size); }

void AliM1535InternalFunction::IoWrite(uint32_t pci_io, uint32_t value, unsigned) {
    Reject("I/O write", pci_io, value);
}

void AliM1535InternalFunction::SaveState(StateWriter& w) {
    StateWriteField field(w);
    PciType0Header::Visit(header_, field);
    PciPowerManagement::Visit(pm_, field);
}

void AliM1535InternalFunction::RestoreState(StateReader& r) {
    StateReadField field(r);
    PciType0Header::Visit(header_, field);
    PciPowerManagement::Visit(pm_, field);
}

}
