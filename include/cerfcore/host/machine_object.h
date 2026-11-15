#pragma once

#include <atomic>
#include <cstdint>
#include <string>

namespace cerfcore {

class MachineObjectTree;
class StateReader;
class StateWriter;

enum class MachineObjectKind : uint8_t {
    kPcCardSocket,
    kSerialPort,
    kStorage,
    kAudio,
    kPhoneLine,
    kBattery,
    kPowerInput,
    kLamp,
    kVibrator,
    kCharacterDisplay,
    kButton,
    kKeyboard,
    kPointer,
    kPointerFeature,
    kSharedFolder,
    kGuestAdditions,
    kUsbFlasher,
    kMediaSocket,
    kCar,
    kVehicleController,
    kTemperatureSensor,
    kDiagnosticsChannel,
    kGuestCalibration,
};

struct ObjectIdentity {
    std::string id;
    std::string name;
};

class MachineObject {
public:
    virtual ~MachineObject() = default;

    virtual std::string       ObjectId() const   = 0;
    virtual std::string       ObjectName() const = 0;
    virtual MachineObjectKind ObjectKind() const = 0;

    virtual void SaveObjectState(StateWriter&) const {}
    virtual void RestoreObjectState(StateReader&) {}

    void     MarkRx() { rx_count_.fetch_add(1, std::memory_order_relaxed); }
    void     MarkTx() { tx_count_.fetch_add(1, std::memory_order_relaxed); }
    uint64_t RxCount() const { return rx_count_.load(std::memory_order_relaxed); }
    uint64_t TxCount() const { return tx_count_.load(std::memory_order_relaxed); }

protected:
    void Changed();
    void Failed(std::string reason, int64_t code = 0, std::string subject = {});

private:
    friend class MachineObjectTree;
    friend class MachineSocket;

    virtual void Republish() { Changed(); }

    std::atomic<MachineObjectTree*> tree_{nullptr};
    std::atomic<uint64_t>           rx_count_{0};
    std::atomic<uint64_t>           tx_count_{0};
};

}
