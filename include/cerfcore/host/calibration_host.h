#pragma once

#include "../core/service.h"
#include "calibration_cross_scan.h"
#include "prompt_answer.h"

#include <cstdint>
#include <functional>
#include <string>

namespace cerfcore {

enum class FrameGrab : uint8_t { kCaptured, kNoFrame, kClosed };

class CalibrationHost : public Service {
public:
    using Service::Service;

    virtual FrameGrab    CaptureSurface(CalibrationFrame& frame)                       = 0;
    virtual PromptAnswer Confirm(const std::string& title, const std::string& message) = 0;
    virtual bool         RunConfigurationJob(const std::function<void()>& job)          = 0;
    virtual void         ShowFallback()                                                = 0;
    virtual void         EndFallback(bool pointer_switched_back)                       = 0;
};

}
