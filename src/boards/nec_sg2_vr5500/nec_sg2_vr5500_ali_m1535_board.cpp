#include "../../peripherals/ali_m1535/ali_m1535_board.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/byte_order.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include "../../socs/vrc5477/vrc5477_intc.h"
#include "nec_sg2_vr5500_family_id.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kBridgeIdselLine   = 18u;
constexpr uint32_t kIntcSourceOfM1535 = 10u;
constexpr uint32_t kIsaIoLast         = 0xFFFFu;

class NecSg2Vr5500AliM1535Board : public AliM1535Board {
public:
    using AliM1535Board::AliM1535Board;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::NecSg2Vr5500;
    }

    void OnReady() override { intc_ = &emu_.Get<Vrc5477Intc>(); }

    uint32_t BridgeIdselLine() const override { return kBridgeIdselLine; }

    void SetInterruptLevel(bool high) override {
        if (high) intc_->AssertSource(kIntcSourceOfM1535);
        else      intc_->DeassertSource(kIntcSourceOfM1535);
    }

    uint32_t ExternalIsaIoRead(uint32_t port, unsigned size) override {
        RequireEmptyIsaIo("read", port, 0u, size);
        return cerf::ByteWidthMask(size);
    }

    void ExternalIsaIoWrite(uint32_t port, uint32_t value, unsigned size) override {
        RequireEmptyIsaIo("write", port, value, size);
    }

private:
    void RequireEmptyIsaIo(const char* what, uint32_t port, uint32_t value, unsigned size) {
        if (port > kIsaIoLast)
            emu_.Get<Fatal>().Die("SG2 ISA bus: external I/O %s port=0x%08X val=0x%08X size=%u", what, port, value,
                                  size);
    }

    Vrc5477Intc* intc_ = nullptr;
};

}

REGISTER_SERVICE_AS(NecSg2Vr5500AliM1535Board, AliM1535Board);

}
