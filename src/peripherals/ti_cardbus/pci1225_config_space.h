#pragma once

#include "../pci/pci_bar.h"
#include "../pci/pci_common_header.h"
#include "../pci/pci_power_management.h"
#include "pci1225_interrupt_terminals.h"

#include <cstdint>

namespace cerfcore {

class StateWriter;
class StateReader;

struct Pci1225SharedConfig {
    uint32_t mfunc               = 0u;
    uint8_t  retry_status        = 0u;
    uint8_t  device_control      = 0u;
    uint8_t  diagnostic_global   = 0u;
    uint16_t bridge_global       = 0u;
    uint32_t system_global       = 0u;
    uint32_t subsystem           = 0u;
    uint32_t legacy_base         = 0u;
    uint8_t  legacy_index        = 0u;
    uint32_t serial_bus          = 0u;
    uint8_t  exca_global_control = 0u;
    uint8_t  gpi_levels          = 0u;

    void Reset(uint32_t mfunc_at_power_on, bool serial_eeprom_present, uint8_t gpi_input_levels);
    uint32_t LegacyBase() const;
    bool SerialBusDetected() const;

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);
};

class Pci1225ConfigSpace {
public:
    Pci1225ConfigSpace(uint8_t function, Pci1225SharedConfig& shared)
        : function_(function), shared_(shared), terminals_(function, shared) {}

    void Reset();

    bool        Read(uint32_t reg, uint32_t* value);
    const char* Write(uint32_t reg, uint32_t value);

    uint8_t  Function() const           { return function_; }
    const Pci1225InterruptTerminals& Terminals() const { return terminals_; }
    bool     MemorySpaceEnabled() const {
        return (common_.Command() & PciCommonHeader::kCommandMemory) != 0u && !pm_.PoweredDown();
    }
    bool     IoSpaceEnabled() const {
        return (common_.Command() & PciCommonHeader::kCommandIo) != 0u && !pm_.PoweredDown();
    }
    uint32_t SocketBarBase() const      { return socket_bar_.base; }
    bool     SocketBarDecodes(uint32_t addr) const { return socket_bar_.Decodes(addr); }

    bool FunctionalInterruptsViaExca() const { return (bridge_control_ & kBcIntr) != 0u; }
    bool CscToPciOnZeroCscSelect() const     { return (diagnostic_ & kDiagCsc) != 0u; }
    bool SmiPending() const;
    bool SmiToCsc() const;
    bool ReducedZoomVideo() const;
    bool IdRegistersWritable() const;

    void SetInterruptFlag()   { card_control_ = static_cast<uint8_t>(card_control_ | kCcIfg); }
    void NoteSocketActivity() { socket_active_ = true; }
    void NotePowerRequestChange(bool vpp12_crossed);

    void SaveState(StateWriter& w);
    void RestoreState(StateReader& r);

private:
    static const PciCommonIdentity kCommonIdentity;
    static constexpr uint32_t kSocketBarWritable = 0xFFFFF000u;
    static constexpr uint16_t kBcIntr       = 0x0080u;
    static constexpr uint8_t  kDiagCsc      = 0x20u;
    static constexpr uint8_t  kCcIfg        = 0x01u;
    static constexpr uint32_t kCapabilityPointer   = 0xA0u;
    static constexpr uint32_t kPowerManagementCaps = 0x7E210001u;
    static constexpr uint32_t kPmcsrBridgeSupport  = 0x00800000u;

    uint8_t                   function_;
    Pci1225SharedConfig&      shared_;
    Pci1225InterruptTerminals terminals_;

    PciCommonHeader common_{kCommonIdentity};
    PciBar   socket_bar_{kSocketBarWritable, 0u, 0u};
    uint32_t bus_numbers_    = 0u;
    uint32_t mem_base_[2]    = {};
    uint32_t mem_limit_[2]   = {};
    uint32_t io_base_[2]     = {};
    uint32_t io_limit_[2]    = {};
    uint8_t  int_line_       = 0u;
    uint16_t bridge_control_ = 0u;
    uint32_t system_socket_  = 0u;
    uint8_t  card_control_   = 0u;
    uint8_t  diagnostic_     = 0u;
    uint32_t socket_dma_[2]  = {};
    PciPowerManagement pm_{kCapabilityPointer, kPowerManagementCaps, kPmcsrBridgeSupport};
    uint16_t gpe_enable_     = 0u;
    uint16_t gpe_status_     = 0u;
    uint8_t  gpo_            = 0u;
    bool     socket_active_  = false;
};

}
