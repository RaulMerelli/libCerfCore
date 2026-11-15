#pragma once

#include "../core/service.h"
#include "host_pointer_input_lock.h"
#include "pointer_source.h"

#include <atomic>

namespace cerfcore {

class TouchInput : public Service, public PointerSource {
public:
    using Service::Service;
    ~TouchInput() override = default;

    void OnPenDown    (int x, int y);
    void OnPenMove    (int x, int y);
    void OnPenUp      (int x, int y);
    void OnCaptureLost();

    void OnPenDown(const HostPointerInputLock::Hold& hold, int x, int y);
    void OnPenMove(const HostPointerInputLock::Hold& hold, int x, int y);
    void OnPenUp  (const HostPointerInputLock::Hold& hold, int x, int y);
    void ReleaseHeld(const HostPointerInputLock::Hold& hold);

    static constexpr char kSourceName[] = "Stock stylus";
    std::string    SourceId()         const override { return "stylus"; }
    std::string    SourceName()       const override { return kSourceName; }
    int            SourcePriority()   const override { return 0; }
    PointerKind    Kind()             const override { return PointerKind::Stylus; }

protected:
    virtual void PenDown    (int x, int y) = 0;
    virtual void PenMove    (int x, int y) = 0;
    virtual void PenUp      (int x, int y) = 0;
    virtual void CaptureLost()             = 0;

private:
    HostPointerInputLock& Lock() const;
    void Down(int x, int y);
    void Move(int x, int y);
    void Up(int x, int y);
    void Lost();

    std::atomic<bool> pen_held_{false};
};

}
