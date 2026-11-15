#pragma once

#include "../core/service.h"
#include "audio_format.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace cerfcore {

class AudioOutputDevice {
public:
    using Consumed = std::function<void(uint32_t block)>;

    virtual ~AudioOutputDevice() = default;

    virtual bool Open(const AudioFormat& format, Consumed consumed)                = 0;
    virtual void Close()                                                           = 0;
    virtual bool Submit(uint32_t block, const uint8_t* bytes, uint32_t length)     = 0;
    virtual void Drop()                                                            = 0;
    virtual bool PlayedFrames(uint64_t& frames) const                              = 0;
};

class AudioOutputs : public Service {
public:
    using Service::Service;

    virtual std::unique_ptr<AudioOutputDevice> CreateOutput(const std::string& stream) = 0;
};

}
