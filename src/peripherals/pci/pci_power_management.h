#pragma once

#include <cstdint>

namespace cerfcore {

struct PciPowerManagement {
    static constexpr uint32_t kD0              = 0x0u;
    static constexpr uint32_t kD1              = 0x1u;
    static constexpr uint32_t kD2              = 0x2u;
    static constexpr uint32_t kD3Hot           = 0x3u;
    static constexpr uint32_t kPowerStateMask  = 0x00000003u;
    static constexpr uint32_t kPmeEnable       = 0x00000100u;
    static constexpr uint32_t kD1Support       = 0x02000000u;
    static constexpr uint32_t kD2Support       = 0x04000000u;
    static constexpr uint32_t kPmeSupport      = 0xF8000000u;
    static constexpr uint32_t kControlStatusOf = 0x4u;

    uint32_t offset         = 0;
    uint32_t capability     = 0;
    uint32_t bridge_support = 0;
    uint32_t power_state    = kD0;

    bool     Owns(uint32_t reg) const { return offset != 0u && (reg == offset || reg == offset + kControlStatusOf); }
    uint32_t Read(uint32_t reg) const { return reg == offset ? capability : (bridge_support | power_state); }
    bool     PoweredDown() const { return power_state != kD0; }
    void     Reset() { power_state = kD0; }

    uint32_t    RequestedState(uint32_t value) const;
    const char* Write(uint32_t reg, uint32_t value);
    const char* WriteHoldingD0(uint32_t reg, uint32_t value);

    template <typename F>
    static constexpr void Visit(PciPowerManagement& p, F& field) {
        field.Skip(p.offset);
        field.Skip(p.capability);
        field.Skip(p.bridge_support);
        field("pm_power_state", p.power_state);
    }
};

}
