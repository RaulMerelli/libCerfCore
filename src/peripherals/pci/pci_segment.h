#pragma once

#include <cstdint>
#include <vector>

namespace cerfcore {

class PciDevice;
class StateReader;
class StateWriter;

class PciSegment {
public:
    explicit PciSegment(const char* name) : name_(name) {}

    void RegisterDevice(PciDevice* dev);
    void RegisterSubtractiveDecoder(PciDevice* dev);

    PciDevice* ConfigDevice(uint8_t dev, uint32_t fnc) const;
    PciDevice* Claimer(bool (PciDevice::*claims)(uint32_t) const, uint32_t addr) const;

    void SaveState(StateWriter& w);
    void RestoreState(StateReader& r);
    void PostRestore();

private:
    const char*             name_;
    std::vector<PciDevice*> devices_;
    PciDevice*              subtractive_ = nullptr;
};

}
