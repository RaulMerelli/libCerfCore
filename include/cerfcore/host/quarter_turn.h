#pragma once

namespace cerfcore {

struct UprightPoint {
    double x;
    double y;
};

inline UprightPoint UprightFromTurned(double x, double y, double turned_w, double turned_h, int quarter) {
    switch (quarter & 3) {
        case 1:  return {turned_h - y, x};
        case 2:  return {turned_w - x, turned_h - y};
        case 3:  return {y, turned_w - x};
        default: return {x, y};
    }
}

}
