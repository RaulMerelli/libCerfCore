#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/machine_object.h>

#include <functional>
#include <optional>

namespace cerfcore {

struct Vr41xxSerialModem {
    Vr41xxSerialModem(ObjectIdentity port_in, int carrier_gpio_in);

    ObjectIdentity port;
    int            carrier_gpio;
};

class Vr41xxSerialWiring : public Service {
public:
    using Service::Service;

    virtual std::optional<Vr41xxSerialModem> ForSiu() const = 0;

    virtual bool SupplyOn() = 0;
    virtual void RegisterSupplyListener(std::function<void()> fn) = 0;
};

class Vr41xxGpioSuppliedSerialWiring : public Vr41xxSerialWiring {
public:
    using Vr41xxSerialWiring::Vr41xxSerialWiring;

    bool SupplyOn() override;
    void RegisterSupplyListener(std::function<void()> fn) override;

protected:
    virtual int SupplyGpio() const = 0;
};

}
