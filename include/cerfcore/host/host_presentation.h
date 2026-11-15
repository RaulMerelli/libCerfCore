#pragma once

#include "../core/service.h"

#include <cstdint>

namespace cerfcore {

class HostPresentation : public Service {
public:
    using Service::Service;

    virtual void PanelEnabled() = 0;
    virtual void GuestRemoded(uint32_t guest_w, uint32_t guest_h) = 0;
    virtual void FrameTurned() = 0;
    virtual void HostModeSet(uint32_t w, uint32_t h) = 0;
    virtual void CustomizationsReset() = 0;

    virtual uint32_t SurfaceWidth() const = 0;
    virtual uint32_t SurfaceHeight() const = 0;
    virtual void     RestoreSurfaceSize(uint32_t w, uint32_t h) = 0;
};

}
