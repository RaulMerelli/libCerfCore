#pragma once

#include "machine_object.h"

namespace cerfcore {

class MachineGuestCalibration : public MachineObject {
public:
    MachineObjectKind ObjectKind() const override { return MachineObjectKind::kGuestCalibration; }

    virtual bool Shown() const              = 0;
    virtual void SwitchToStockPointer()     = 0;
};

}
