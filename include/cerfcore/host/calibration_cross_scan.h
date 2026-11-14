#pragma once

#include <cstdint>
#include <vector>

namespace cerfcore {

enum class CalibrationZone : uint8_t { kCenter, kTopLeft, kBottomLeft, kBottomRight, kTopRight, kNone };

struct CalibrationFrame {
    std::vector<uint32_t> pixels;
    uint32_t              width  = 0;
    uint32_t              height = 0;

    bool operator==(const CalibrationFrame&) const = default;
};

struct CalibrationCross {
    int             hole_x  = 0;
    int             hole_y  = 0;
    int             press_x = 0;
    int             press_y = 0;
    int             left    = 0;
    int             top     = 0;
    int             right   = 0;
    int             bottom  = 0;
    int             quarter = 0;
    CalibrationZone zone    = CalibrationZone::kNone;
};

enum class CrossScanStatus : uint8_t { kFound, kNone, kAmbiguous, kOffZone };

class CalibrationCrossScan {
public:
    CrossScanStatus Find(const CalibrationFrame& frame, CalibrationCross& out);
    CrossScanStatus FindChanged(const CalibrationFrame& before, const CalibrationFrame& after,
                                const CalibrationCross& old, CalibrationCross& out);

    static bool            BoxChanged(const CalibrationFrame& before, const CalibrationFrame& after,
                                      const CalibrationCross& cross);
    static CalibrationZone NextZone(CalibrationZone zone);
    static int             RemainingAfter(CalibrationZone zone);
    static const char*     ZoneName(CalibrationZone zone);
    static const char*     StatusName(CrossScanStatus status);

private:
    struct Arms {
        int left;
        int right;
        int top;
        int bottom;
    };

    struct Layout {
        Arms arms;
        int  quarter;
    };

    void BuildMasks(const CalibrationFrame& frame);
    bool Match(const CalibrationFrame& frame, int cx, int cy, const Arms& arms) const;
    bool HoleMatches(const CalibrationFrame& frame, int cx, int cy) const;
    bool StrokeH(int x, int y) const { return mask_h_[static_cast<size_t>(y) * width_ + x] != 0; }
    bool StrokeV(int x, int y) const { return mask_v_[static_cast<size_t>(y) * width_ + x] != 0; }

    static CalibrationCross Describe(int cx, int cy, const Layout& layout, uint32_t w, uint32_t h);
    static CalibrationZone  ZoneOf(int x, int y, uint32_t w, uint32_t h, int quarter);
    static CrossScanStatus  Decide(const std::vector<CalibrationCross>& found, CalibrationCross& out);

    std::vector<uint8_t> mask_h_;
    std::vector<uint8_t> mask_v_;
    uint32_t             width_  = 0;
    uint32_t             height_ = 0;
    CalibrationFrame     changed_;
};

}
