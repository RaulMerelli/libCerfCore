#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/frame_renderer.h>
#include "guest_additions_frame_renderer.h"
#include "panel_frame_renderer.h"

#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

namespace cerfcore {

namespace {

class PresentedFrameRenderer : public FrameRenderer {
public:
    using FrameRenderer::FrameRenderer;

    bool HasFrame() override {
        std::lock_guard<std::mutex> lk(layers_mtx_);
        auto* ga = emu_.TryGet<GuestAdditionsFrameRenderer>();
        if (ga && ga->HasFrame()) return true;
        auto* panel = emu_.TryGet<PanelFrameRenderer>();
        return panel && panel->HasFrame();
    }

    void PresentedSize(uint32_t& w, uint32_t& h) override {
        std::lock_guard<std::mutex> lk(layers_mtx_);
        if (auto* ga = emu_.TryGet<GuestAdditionsFrameRenderer>()) {
            ga->PresentedSize(w, h);
            return;
        }
        if (auto* panel = emu_.TryGet<PanelFrameRenderer>()) {
            panel->PresentedSize(w, h);
            return;
        }
        w = 0;
        h = 0;
    }

    uint32_t DisplayQuarter() override {
        std::lock_guard<std::mutex> lk(layers_mtx_);
        if (auto* ga = emu_.TryGet<GuestAdditionsFrameRenderer>()) return ga->DisplayQuarter();
        if (auto* panel = emu_.TryGet<PanelFrameRenderer>()) return panel->DisplayQuarter();
        return 0;
    }

    void RenderInto(uint32_t* dib_bgra32,
                    uint32_t  width,
                    uint32_t  height) override {
        std::lock_guard<std::mutex> lk(layers_mtx_);
        auto* ga = emu_.TryGet<GuestAdditionsFrameRenderer>();
        if (ga && ga->HasFrame()) {
            ga->RenderInto(dib_bgra32, width, height);
            return;
        }

        auto* panel = emu_.TryGet<PanelFrameRenderer>();
        if (!panel || !panel->HasFrame()) {
            std::memset(dib_bgra32, 0, (size_t)width * height * 4u);
            return;
        }
        if (!ga) {
            panel->RenderInto(dib_bgra32, width, height);
            return;
        }
        ComposeLayer(dib_bgra32, width, height, *panel);
    }

private:
    void ComposeLayer(uint32_t* dst, uint32_t dst_w, uint32_t dst_h,
                      FrameRenderer& layer) {
        uint32_t lw = 0, lh = 0;
        layer.PresentedSize(lw, lh);
        if (lw == 0 || lh == 0) {
            std::memset(dst, 0, (size_t)dst_w * dst_h * 4u);
            return;
        }
        if (lw == dst_w && lh == dst_h) {
            layer.RenderInto(dst, dst_w, dst_h);
            return;
        }

        if (lw != scratch_w_ || lh != scratch_h_) {
            scratch_.assign((size_t)lw * lh, 0u);
            scratch_w_ = lw;
            scratch_h_ = lh;
        }
        layer.RenderInto(scratch_.data(), lw, lh);

        if (lw != map_src_w_ || lh != map_src_h_ || dst_w != map_dst_w_ || dst_h != map_dst_h_) {
            uint32_t fit_w, fit_h;
            if ((uint64_t)dst_w * lh <= (uint64_t)dst_h * lw) {
                fit_w = dst_w;
                fit_h = (uint32_t)(((uint64_t)lh * dst_w) / lw);
            } else {
                fit_h = dst_h;
                fit_w = (uint32_t)(((uint64_t)lw * dst_h) / lh);
            }
            if (fit_w == 0) fit_w = 1;
            if (fit_h == 0) fit_h = 1;
            src_col_.resize(fit_w);
            src_row_.resize(fit_h);
            for (uint32_t x = 0; x < fit_w; ++x)
                src_col_[x] = (uint32_t)(((2ull * x + 1u) * lw) / (2ull * fit_w));
            for (uint32_t y = 0; y < fit_h; ++y)
                src_row_[y] = (uint32_t)(((2ull * y + 1u) * lh) / (2ull * fit_h));
            map_src_w_ = lw;
            map_src_h_ = lh;
            map_dst_w_ = dst_w;
            map_dst_h_ = dst_h;
        }

        const uint32_t out_w = (uint32_t)src_col_.size();
        const uint32_t out_h = (uint32_t)src_row_.size();
        const uint32_t dst_x = (dst_w - out_w) / 2u;
        const uint32_t dst_y = (dst_h - out_h) / 2u;

        std::memset(dst, 0, (size_t)dst_w * dst_h * 4u);

        const uint32_t* prev_src_row = nullptr;
        const uint32_t* prev_dst_row = nullptr;
        for (uint32_t y = 0; y < out_h; ++y) {
            const uint32_t* src_row = scratch_.data() + (size_t)src_row_[y] * lw;
            uint32_t* dst_row = dst + (size_t)(dst_y + y) * dst_w + dst_x;
            if (src_row == prev_src_row) {
                std::memcpy(dst_row, prev_dst_row, (size_t)out_w * 4u);
                continue;
            }
            for (uint32_t x = 0; x < out_w; ++x)
                dst_row[x] = src_row[src_col_[x]];
            prev_src_row = src_row;
            prev_dst_row = dst_row;
        }
    }

    std::mutex            layers_mtx_;
    std::vector<uint32_t> scratch_;
    uint32_t scratch_w_ = 0;
    uint32_t scratch_h_ = 0;
    std::vector<uint32_t> src_col_;
    std::vector<uint32_t> src_row_;
    uint32_t map_src_w_ = 0;
    uint32_t map_src_h_ = 0;
    uint32_t map_dst_w_ = 0;
    uint32_t map_dst_h_ = 0;
};

}

REGISTER_SERVICE_AS(PresentedFrameRenderer, FrameRenderer);

}
