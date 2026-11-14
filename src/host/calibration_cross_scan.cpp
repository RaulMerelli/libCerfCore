#include <cerfcore/host/calibration_cross_scan.h>

#include <cerfcore/host/quarter_turn.h>

#include "../core/bit_field.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iterator>

namespace cerfcore {

namespace {

constexpr int      kInk         = 10;
constexpr int      kBackground  = 24;
constexpr int      kInverse     = 12;
constexpr uint32_t kArmDivisor  = 20;
constexpr int      kMinArm      = 4;
constexpr int      kStroke      = 2;
constexpr uint32_t kRgbMask     = 0x00FFFFFFu;
constexpr uint32_t kChangedInk  = 0xFF000000u;
constexpr uint32_t kUnchanged   = 0xFFFFFFFFu;
constexpr double   kZoneDivisor = 8.0;

constexpr CalibrationZone kZoneOrder[] = {CalibrationZone::kCenter, CalibrationZone::kTopLeft,
                                          CalibrationZone::kBottomLeft, CalibrationZone::kBottomRight,
                                          CalibrationZone::kTopRight};

size_t OrderIndex(CalibrationZone zone) {
    return static_cast<size_t>(std::find(std::begin(kZoneOrder), std::end(kZoneOrder), zone) - std::begin(kZoneOrder));
}

struct Rgb {
    int r;
    int g;
    int b;
};

Rgb Unpack(uint32_t p) {
    return {static_cast<int>(cerf::BitField(p, 16, 0xFFu)), static_cast<int>(cerf::BitField(p, 8, 0xFFu)),
            static_cast<int>(cerf::BitField(p, 0, 0xFFu))};
}

int Distance(Rgb a, Rgb b) { return std::max({std::abs(a.r - b.r), std::abs(a.g - b.g), std::abs(a.b - b.b)}); }

int InverseDistance(Rgb a, Rgb b) {
    return std::max({std::abs(a.r - (255 - b.r)), std::abs(a.g - (255 - b.g)), std::abs(a.b - (255 - b.b))});
}

bool InvertsEither(Rgb s, Rgb fa, Rgb fb) {
    return InverseDistance(s, fa) <= kInverse || InverseDistance(s, fb) <= kInverse;
}

uint8_t IsStroke(uint32_t fa_px, uint32_t s0_px, uint32_t s1_px, uint32_t fb_px) {
    const Rgb fa       = Unpack(fa_px);
    const Rgb s0       = Unpack(s0_px);
    int       contrast = Distance(s0, fa);
    if (contrast < kInk) return 0;
    const Rgb fb = Unpack(fb_px);
    const Rgb s1 = Unpack(s1_px);
    contrast     = std::min({contrast, Distance(s0, fb), Distance(s1, fa), Distance(s1, fb)});
    if (contrast < kInk) return 0;
    const int flank = Distance(fa, fb);
    const int inner = Distance(s1, s0);
    if (flank <= kBackground && flank * 3 <= contrast && inner * 3 <= contrast) return 1;
    return InvertsEither(s0, fa, fb) && InvertsEither(s1, fa, fb) ? 1 : 0;
}

}

void CalibrationCrossScan::BuildMasks(const CalibrationFrame& frame) {
    width_          = frame.width;
    height_         = frame.height;
    const int    w  = static_cast<int>(width_);
    const int    h  = static_cast<int>(height_);
    const size_t n  = static_cast<size_t>(w) * h;
    const auto*  px = frame.pixels.data();
    mask_h_.assign(n, 0);
    mask_v_.assign(n, 0);
    for (int y = 1; y + kStroke < h; ++y) {
        const uint32_t* up   = px + static_cast<size_t>(y - 1) * w;
        const uint32_t* row0 = up + w;
        const uint32_t* row1 = row0 + w;
        const uint32_t* down = row1 + w;
        uint8_t*        out  = &mask_h_[static_cast<size_t>(y) * w];
        for (int x = 0; x < w; ++x) out[x] = IsStroke(up[x], row0[x], row1[x], down[x]);
    }
    for (int y = 0; y < h; ++y) {
        const uint32_t* row = px + static_cast<size_t>(y) * w;
        uint8_t*        out = &mask_v_[static_cast<size_t>(y) * w];
        for (int x = 1; x + kStroke < w; ++x) out[x] = IsStroke(row[x - 1], row[x], row[x + 1], row[x + 2]);
    }
}

bool CalibrationCrossScan::Match(const CalibrationFrame& frame, int cx, int cy, const Arms& arms) const {
    const int w = static_cast<int>(width_);
    const int h = static_cast<int>(height_);

    const int left_end = std::max(cx - arms.left, 0);
    if ((cx - left_end) * 2 < arms.left) return false;
    for (int x = cx - 1; x >= left_end; --x)
        if (!StrokeH(x, cy)) return false;
    const int right_end = std::min(cx + kStroke + arms.right, w);
    if ((right_end - cx - kStroke) * 2 < arms.right) return false;
    for (int x = cx + kStroke; x < right_end; ++x)
        if (!StrokeH(x, cy)) return false;
    const int top_end = std::max(cy - arms.top, 0);
    if ((cy - top_end) * 2 < arms.top) return false;
    for (int y = cy - 1; y >= top_end; --y)
        if (!StrokeV(cx, y)) return false;
    const int bottom_end = std::min(cy + kStroke + arms.bottom, h);
    if ((bottom_end - cy - kStroke) * 2 < arms.bottom) return false;
    for (int y = cy + kStroke; y < bottom_end; ++y)
        if (!StrokeV(cx, y)) return false;

    if (cx - arms.left - 1 >= 0 && StrokeH(cx - arms.left - 1, cy)) return false;
    if (cx + kStroke + arms.right < w && StrokeH(cx + kStroke + arms.right, cy)) return false;
    if (cy - arms.top - 1 >= 0 && StrokeV(cx, cy - arms.top - 1)) return false;
    if (cy + kStroke + arms.bottom < h && StrokeV(cx, cy + kStroke + arms.bottom)) return false;
    return HoleMatches(frame, cx, cy);
}

bool CalibrationCrossScan::HoleMatches(const CalibrationFrame& frame, int cx, int cy) const {
    const int  w  = static_cast<int>(width_);
    const auto at = [&](int x, int y) { return Unpack(frame.pixels[static_cast<size_t>(y) * w + x]); };
    const Rgb  diag[] = {at(cx - 1, cy - 1), at(cx + kStroke, cy - 1), at(cx - 1, cy + kStroke),
                         at(cx + kStroke, cy + kStroke)};
    for (int dy = 0; dy < kStroke; ++dy) {
        for (int dx = 0; dx < kStroke; ++dx) {
            const Rgb p      = at(cx + dx, cy + dy);
            const Rgb arms[] = {at(cx - 1, cy + dy), at(cx + kStroke, cy + dy), at(cx + dx, cy - 1),
                                at(cx + dx, cy + kStroke)};
            bool      ok     = false;
            for (const Rgb& q : diag) ok = ok || Distance(p, q) <= kBackground;
            for (const Rgb& q : arms) ok = ok || InverseDistance(p, q) <= kInverse;
            if (!ok) return false;
        }
    }
    return true;
}

CrossScanStatus CalibrationCrossScan::Find(const CalibrationFrame& frame, CalibrationCross& out) {
    const uint32_t fw  = frame.width;
    const uint32_t fh  = frame.height;
    const int      arm = static_cast<int>(std::min(fw, fh) / kArmDivisor);
    if (arm < kMinArm) return CrossScanStatus::kNone;
    BuildMasks(frame);

    const int    w         = static_cast<int>(fw);
    const int    h         = static_cast<int>(fh);
    const int    long_arm  = arm;
    const int    short_arm = arm - 1;
    const Layout layouts[] = {{{long_arm, short_arm, long_arm, short_arm}, 0},
                              {{long_arm, short_arm, short_arm, long_arm}, 1},
                              {{short_arm, long_arm, short_arm, long_arm}, 2},
                              {{short_arm, long_arm, long_arm, short_arm}, 3}};
    std::vector<CalibrationCross> found;
    for (int y = 1; y + kStroke + 1 < h; ++y) {
        for (int x = 1; x + kStroke + 1 < w; ++x) {
            if (!StrokeH(x - 1, y) || !StrokeV(x, y - 1)) continue;
            for (const Layout& layout : layouts)
                if (Match(frame, x, y, layout.arms)) found.push_back(Describe(x, y, layout, fw, fh));
        }
    }
    return Decide(found, out);
}

CalibrationCross CalibrationCrossScan::Describe(int cx, int cy, const Layout& layout, uint32_t w, uint32_t h) {
    const Arms&      a = layout.arms;
    const int        iw = static_cast<int>(w);
    const int        ih = static_cast<int>(h);
    CalibrationCross c;
    c.hole_x  = cx;
    c.hole_y  = cy;
    c.press_x = cx + (a.left < a.right ? 1 : 0);
    c.press_y = cy + (a.top < a.bottom ? 1 : 0);
    c.left    = cx - std::min(a.left, cx);
    c.top     = cy - std::min(a.top, cy);
    c.right   = cx + kStroke - 1 + std::min(a.right, iw - cx - kStroke);
    c.bottom  = cy + kStroke - 1 + std::min(a.bottom, ih - cy - kStroke);
    c.quarter = layout.quarter;
    c.zone    = ZoneOf(c.press_x, c.press_y, w, h, layout.quarter);
    return c;
}

CrossScanStatus CalibrationCrossScan::FindChanged(const CalibrationFrame& before, const CalibrationFrame& after,
                                                  const CalibrationCross& old, CalibrationCross& out) {
    if (before.width != after.width || before.height != after.height) return CrossScanStatus::kNone;
    const size_t n = static_cast<size_t>(after.width) * after.height;
    changed_.width  = after.width;
    changed_.height = after.height;
    changed_.pixels.resize(n);
    for (size_t i = 0; i < n; ++i)
        changed_.pixels[i] = ((before.pixels[i] ^ after.pixels[i]) & kRgbMask) != 0 ? kChangedInk : kUnchanged;
    const int w = static_cast<int>(after.width);
    for (int y = old.hole_y; y < old.hole_y + kStroke; ++y)
        for (int x = old.left; x <= old.right; ++x) changed_.pixels[static_cast<size_t>(y) * w + x] = kUnchanged;
    for (int y = old.top; y <= old.bottom; ++y)
        for (int x = old.hole_x; x < old.hole_x + kStroke; ++x)
            changed_.pixels[static_cast<size_t>(y) * w + x] = kUnchanged;
    return Find(changed_, out);
}

bool CalibrationCrossScan::BoxChanged(const CalibrationFrame& before, const CalibrationFrame& after,
                                      const CalibrationCross& cross) {
    if (before.width != after.width || before.height != after.height) return true;
    const size_t w = after.width;
    for (int y = cross.top; y <= cross.bottom; ++y)
        for (int x = cross.left; x <= cross.right; ++x) {
            const size_t i = static_cast<size_t>(y) * w + x;
            if (((before.pixels[i] ^ after.pixels[i]) & kRgbMask) != 0) return true;
        }
    return false;
}

CalibrationZone CalibrationCrossScan::ZoneOf(int x, int y, uint32_t frame_w, uint32_t frame_h, int quarter) {
    const bool         turned = (quarter & 1) != 0;
    const double       w      = turned ? frame_h : frame_w;
    const double       h      = turned ? frame_w : frame_h;
    const UprightPoint at     = UprightFromTurned(x + 0.5, y + 0.5, frame_w, frame_h, quarter);
    const double       dx     = at.x - w / 2.0;
    const double       dy     = at.y - h / 2.0;
    const bool         near_x = std::fabs(dx) < w / kZoneDivisor;
    const bool         near_y = std::fabs(dy) < h / kZoneDivisor;
    if (near_x && near_y) return CalibrationZone::kCenter;
    if (near_x || near_y) return CalibrationZone::kNone;
    if (dy < 0) return dx < 0 ? CalibrationZone::kTopLeft : CalibrationZone::kTopRight;
    return dx < 0 ? CalibrationZone::kBottomLeft : CalibrationZone::kBottomRight;
}

CrossScanStatus CalibrationCrossScan::Decide(const std::vector<CalibrationCross>& found, CalibrationCross& out) {
    if (found.empty()) return CrossScanStatus::kNone;
    size_t                  zoned = 0;
    const CalibrationCross* pick  = nullptr;
    for (const CalibrationCross& c : found) {
        if (c.zone == CalibrationZone::kNone) continue;
        ++zoned;
        pick = &c;
    }
    if (zoned == 0) return CrossScanStatus::kOffZone;
    if (zoned != found.size() || zoned > 1) return CrossScanStatus::kAmbiguous;
    out = *pick;
    return CrossScanStatus::kFound;
}

CalibrationZone CalibrationCrossScan::NextZone(CalibrationZone zone) {
    const size_t at = OrderIndex(zone);
    return at + 1 < std::size(kZoneOrder) ? kZoneOrder[at + 1] : CalibrationZone::kNone;
}

int CalibrationCrossScan::RemainingAfter(CalibrationZone zone) {
    const size_t at = OrderIndex(zone);
    return at < std::size(kZoneOrder) ? static_cast<int>(std::size(kZoneOrder) - 1 - at) : 0;
}

const char* CalibrationCrossScan::ZoneName(CalibrationZone zone) {
    switch (zone) {
        case CalibrationZone::kCenter:      return "center";
        case CalibrationZone::kTopLeft:     return "top-left";
        case CalibrationZone::kBottomLeft:  return "bottom-left";
        case CalibrationZone::kBottomRight: return "bottom-right";
        case CalibrationZone::kTopRight:    return "top-right";
        case CalibrationZone::kNone:        return "none";
    }
    return "none";
}

const char* CalibrationCrossScan::StatusName(CrossScanStatus status) {
    switch (status) {
        case CrossScanStatus::kFound:     return "found";
        case CrossScanStatus::kNone:      return "none";
        case CrossScanStatus::kAmbiguous: return "ambiguous";
        case CrossScanStatus::kOffZone:   return "off-zone";
    }
    return "none";
}

}
