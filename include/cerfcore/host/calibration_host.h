#pragma once

#include "../core/service.h"

#include <cstdint>

namespace cerfcore {

class CalibrationHost : public Service {
public:
    using Service::Service;

    virtual bool      ApprovesCalibration()                                = 0;
    virtual bool      ApprovesPanelResolution(uint32_t width, uint32_t height) = 0;
    virtual void      CalibrationFinished()                                = 0;
    virtual void      CalibrationFailed()                                  = 0;
    virtual void      GuestCalibrationUnhandled()                          = 0;
    virtual void      GuestCalibrationEnded(bool guest_pointer_restored)   = 0;
};

}
