#pragma once

#include <cerfcore/host/touch_input.h>

#include <atomic>

namespace cerfcore {

class Ucb1x00TouchPanel : public TouchInput {
public:
    using TouchInput::TouchInput;

    bool ShouldRegister() override;

    bool Down();
    int  X() const { return x_.load(std::memory_order_relaxed); }
    int  Y() const { return y_.load(std::memory_order_relaxed); }

protected:
    void PenDown(int x, int y) override;
    void PenMove(int x, int y) override;
    void PenUp  (int x, int y) override;
    void CaptureLost() override;

private:
    void SetPen(bool down, int x, int y);

    std::atomic<int> x_{0}, y_{0};
};

}
