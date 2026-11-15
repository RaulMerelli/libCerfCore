#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/calibration_cross_scan.h>

namespace cerfcore {

class FrameRenderer;
class HostPresentation;

class CalibrationFrameSource : public Service {
public:
    using Service::Service;

    void OnReady() override;
    bool Grab(CalibrationFrame& frame);

private:
    FrameRenderer*    renderer_     = nullptr;
    HostPresentation* presentation_ = nullptr;
};

}
