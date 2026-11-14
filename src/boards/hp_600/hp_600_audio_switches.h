#pragma once

#include <cerfcore/core/service.h>

#include <functional>

namespace cerfcore {

class Hp600AudioSwitches : public Service {
public:
    using Service::Service;

    virtual bool SpeakerOn() const                                 = 0;
    virtual bool MicrophoneOn() const                              = 0;
    virtual void AddListener(std::function<void()> listener)      = 0;
};

}
