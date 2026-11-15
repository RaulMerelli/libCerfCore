#pragma once

#include "../core/service.h"
#include "host_pointer_event.h"
#include "machine_object.h"
#include "source_selection.h"

#include <cstdint>
#include <string>

namespace cerfcore {

class PointerSource;

class PointerRouter : public Service, public MachineObject, public SourceSelection<PointerSource> {
public:
    using Service::Service;

    void OnReady() override;

    std::string       ObjectId() const override { return "pointer"; }
    static constexpr char kObjectName[] = "Pointing device";
    std::string       ObjectName() const override { return kObjectName; }
    MachineObjectKind ObjectKind() const override { return MachineObjectKind::kPointer; }
    void              SaveObjectState(StateWriter& w) const override;
    void              RestoreObjectState(StateReader& r) override;

    bool     Route(const HostPointerEvent& event);
    void     ReleaseLost(const HostPointerEvent& event);
    void     OnRelativeMove(int dx, int dy, uint32_t buttons);
    uint32_t RelativeButtons() const { return relative_buttons_; }
    bool     PressHeld() const { return pen_down_ || pointer_buttons_ != 0; }
    void     ReleasePen();
    void     ReleasePointerButtons();
    void     ReleaseRelative();

    static uint32_t RelativeButtonsOf(uint32_t held);

private:
    void            SelectionChanged() override { Changed(); }
    bool            RouteStylus(const HostPointerEvent& event);
    bool            RoutePointer(const HostPointerEvent& event);
    void            LiftPen(const HostPointerEvent& event);
    static uint32_t PointerButtonsOf(uint32_t held);

    bool     pen_down_         = false;
    uint32_t pointer_buttons_  = 0;
    uint32_t relative_buttons_ = 0;
};

}
