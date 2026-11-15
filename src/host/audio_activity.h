#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/machine_object.h>

#include <string>

namespace cerfcore {

class AudioActivity : public Service, public MachineObject {
public:
    using Service::Service;

    void NotePresent();

    std::string       ObjectId() const override { return "audio"; }
    static constexpr char kObjectName[] = "Audio";
    std::string       ObjectName() const override { return kObjectName; }
    MachineObjectKind ObjectKind() const override { return MachineObjectKind::kAudio; }

private:
    bool added_ = false;
};

}
