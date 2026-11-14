#include "pci_segment.h"

#include <cerfcore/state/state_stream.h>
#include "pci_device.h"

namespace cerfcore {

void PciSegment::RegisterDevice(PciDevice* dev) { devices_.push_back(dev); }

void PciSegment::RegisterSubtractiveDecoder(PciDevice* dev) {
    subtractive_ = dev;
    devices_.push_back(dev);
}

PciDevice* PciSegment::ConfigDevice(uint8_t dev, uint32_t fnc) const {
    for (PciDevice* d : devices_)
        if (d->PciDev() == dev && d->DecodesFunction(fnc) && d->ConfigClaims()) return d;
    return nullptr;
}

PciDevice* PciSegment::Claimer(bool (PciDevice::*claims)(uint32_t) const, uint32_t addr) const {
    for (PciDevice* d : devices_)
        if (d != subtractive_ && (d->*claims)(addr)) return d;
    if (subtractive_ && (subtractive_->*claims)(addr)) return subtractive_;
    return nullptr;
}

void PciSegment::SaveState(StateWriter& w) {
    w.Write<uint32_t>("devices_count", static_cast<uint32_t>(devices_.size()));
    for (PciDevice* d : devices_) {
        w.BeginFrame(static_cast<uint32_t>((d->PciDev() << 8) | d->PciFnc()));
        d->SaveState(w);
        w.EndFrame();
    }
}

void PciSegment::RestoreState(StateReader& r) {
    uint32_t n = 0;
    r.Read("devices_count", n);
    if (n != devices_.size()) r.Reject("PCI %s: %u devices, this build has %zu", name_, n, devices_.size());
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t tag   = r.EnterFrame();
        const uint8_t  dev   = static_cast<uint8_t>(tag >> 8);
        const uint8_t  fnc   = static_cast<uint8_t>(tag & 0xFFu);
        PciDevice*     match = nullptr;
        for (PciDevice* d : devices_) {
            if (d->PciDev() == dev && d->PciFnc() == fnc) {
                match = d;
                break;
            }
        }
        if (!match) r.Reject("PCI %s: no device dev=%u fnc=%u in this build", name_, dev, fnc);
        match->RestoreState(r);
        r.LeaveFrame();
    }
}

void PciSegment::PostRestore() {
    for (PciDevice* d : devices_) d->PostRestore();
}

}
