#include "pci_power_management.h"

namespace cerfcore {

uint32_t PciPowerManagement::RequestedState(uint32_t value) const {
    const uint32_t s = value & kPowerStateMask;
    if ((s == kD1 && (capability & kD1Support) == 0u) || (s == kD2 && (capability & kD2Support) == 0u))
        return power_state;
    return s;
}

const char* PciPowerManagement::Write(uint32_t reg, uint32_t value) {
    if (reg == offset) return nullptr;
    if ((value & kPmeEnable) != 0u && (capability & kPmeSupport) != 0u)
        return "sets PME_En, whose PME# events are not modeled";
    const uint32_t next = RequestedState(value);
    if (power_state == kD3Hot && next == kD0)
        return "returns the function from D3hot to D0, whose internal reset is not modeled";
    power_state = next;
    return nullptr;
}

const char* PciPowerManagement::WriteHoldingD0(uint32_t reg, uint32_t value) {
    if (reg != offset && RequestedState(value) != kD0) return "enters a power state other than D0";
    return Write(reg, value);
}

}
