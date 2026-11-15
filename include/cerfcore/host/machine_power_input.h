#pragma once

#include "machine_object.h"

namespace cerfcore {

class MachinePowerInput : public MachineObject {
public:
    MachineObjectKind ObjectKind() const override { return MachineObjectKind::kPowerInput; }

    virtual bool PowerFailed() const         = 0;
    virtual void SetPowerFailed(bool failed) = 0;
};

}
