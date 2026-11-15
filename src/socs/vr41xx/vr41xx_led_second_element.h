#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/machine_indicators.h>

#include <string>

namespace cerfcore {

class Vr41xxLedSecondElement : public Service, public MachineLamp {
public:
    using Service::Service;

    virtual std::string PackageName() const = 0;
    virtual std::string ElementName() const = 0;
    virtual ObjectRgb   LitColor() const = 0;
    virtual bool        Lit() const = 0;

    std::string ObjectId() const override { return "second_element"; }
    std::string ObjectName() const override { return ElementName(); }
    LampState   Lamp() const override { return LampState{Lit(), false, LitColor()}; }
};

}
