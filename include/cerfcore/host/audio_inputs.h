#pragma once

#include "../core/service.h"
#include "audio_format.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace cerfcore {

class AudioInputDevice {
public:
    using Captured = std::function<void(const uint8_t* bytes, size_t length)>;

    virtual ~AudioInputDevice() = default;

    virtual bool Open(const AudioFormat& format, Captured captured) = 0;
    virtual void Close()                                            = 0;
};

class AudioInputs : public Service {
public:
    using Service::Service;

    virtual std::unique_ptr<AudioInputDevice> CreateInput(const std::string& stream) = 0;
};

}
