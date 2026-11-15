#pragma once

#include "machine_object.h"

namespace cerfcore {

class MachinePhoneLine : public MachineObject {
public:
    MachineObjectKind ObjectKind() const override { return MachineObjectKind::kPhoneLine; }

    virtual bool Plugged() const              = 0;
    virtual void SetPlugged(bool plugged)     = 0;
};

}
