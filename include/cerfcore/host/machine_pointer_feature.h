#pragma once

#include "machine_object.h"
#include "published_state.h"

namespace cerfcore {

class MachinePointerFeature : public MachineObject {
public:
    MachineObjectKind ObjectKind() const override { return MachineObjectKind::kPointerFeature; }

    virtual bool Available() const      = 0;
    virtual bool Enabled() const        = 0;
    virtual void SetEnabled(bool on)    = 0;

protected:
    void Refresh();

private:
    struct Published {
        bool available = false;
        bool enabled   = false;
        bool operator==(const Published&) const = default;
    };

    void Republish() override { Refresh(); }

    PublishedState<Published> published_;
};

}
