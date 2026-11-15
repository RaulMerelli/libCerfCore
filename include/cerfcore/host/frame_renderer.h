#pragma once

#include "../core/service.h"
#include "frame_source.h"

#include <cstdint>

namespace cerfcore {

class FrameRenderer : public Service, public FrameSource {
public:
    using Service::Service;
    ~FrameRenderer() override = default;

    bool HasFrame() override = 0;

    void RenderInto(uint32_t* dib_bgra32,
                    uint32_t  width,
                    uint32_t  height) override = 0;

    virtual void PresentedSize(uint32_t& w, uint32_t& h) = 0;

    virtual uint32_t DisplayQuarter() { return 0; }
};

}
