#include "hp_f1252a_card.h"

#include "../pcmcia/pcmcia_slot.h"

#include <cerfcore/core/byte_order.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/log.h>
#include <cerfcore/host/external_displays.h>
#include <cerfcore/state/state_stream.h>

namespace cerfcore {

namespace {

const uint8_t kCisData[] = {
    0x01, 0x03, 0xDC, 0x00, 0xFF,
    0x15, 0x12, 0x04, 0x01,
        'H', 'P', 0x00,
        'P', 'a', 'l', 'm', 't', 'o', 'p', ' ', 'V', 'G', 'A', 0x00,
        0xFF,
    0x21, 0x02, 0xFF, 0x00,
    0xFF, 0x00,
};
constexpr size_t kCisSize = sizeof(kCisData);

const uint8_t kMagic[8] = { 0x55, 0x00, 0x40, 0x00, 0x61, 0x00, 0x34, 0x00 };

struct DetectProbeReadRange {
    uint32_t first;
    uint32_t last;
};

constexpr DetectProbeReadRange kDetectProbeReads[] = {
    {0x00000u, 0x00200u}, {0x00230u, 0x00230u}, {0x00240u, 0x00240u}, {0x00280u, 0x00280u},
    {0x002A0u, 0x002A0u}, {0x00300u, 0x00300u}, {0x00310u, 0x00310u}, {0x00380u, 0x00380u},
    {0x00400u, 0x00400u}, {0x01007u, 0x01007u}, {0x80000u, 0x8000Eu},
};

struct DetectProbeWrite {
    uint32_t first;
    uint32_t last;
    uint8_t  count;
    uint8_t  values[5];
};

constexpr DetectProbeWrite kDetectProbeWrites[] = {
    {0x00000u, 0x00000u, 5, {0x00, 0x35, 0x90, 0xF0, 0xFF}},
    {0x00001u, 0x00001u, 5, {0x00, 0x69, 0x90, 0xF0, 0xFF}},
    {0x00002u, 0x00002u, 5, {0x00, 0x90, 0xAC, 0xF0, 0xFF}},
    {0x00003u, 0x00003u, 5, {0x00, 0x63, 0x90, 0xF0, 0xFF}},
    {0x00004u, 0x0000Eu, 3, {0x90, 0xF0, 0xFF}},
    {0x00055u, 0x00055u, 2, {0x98, 0xFF}},
    {0x000AAu, 0x000AAu, 2, {0x98, 0xFF}},
    {0x000ABu, 0x000ABu, 1, {0x00}},
    {0x000FFu, 0x000FFu, 2, {0x98, 0xFF}},
    {0x00154u, 0x00154u, 2, {0x98, 0xFF}},
    {0x001A9u, 0x001A9u, 2, {0x98, 0xFF}},
    {0x001FEu, 0x001FEu, 2, {0x98, 0xFF}},
    {0x00253u, 0x00253u, 2, {0x98, 0xFF}},
    {0x002A8u, 0x002A8u, 2, {0x98, 0xFF}},
    {0x00400u, 0x00401u, 1, {0x00}},
    {0x01002u, 0x01002u, 1, {0x85}},
    {0x01003u, 0x01003u, 1, {0x00}},
    {0x01005u, 0x01005u, 1, {0x00}},
    {0x02AAAu, 0x02AAAu, 1, {0x55}},
    {0x05554u, 0x05554u, 1, {0x55}},
    {0x05555u, 0x05555u, 3, {0x55, 0x90, 0xAA}},
    {0x0AAA8u, 0x0AAA9u, 1, {0x55}},
    {0x0AAAAu, 0x0AAABu, 3, {0x55, 0x90, 0xAA}},
    {0x15554u, 0x15557u, 2, {0x90, 0xAA}},
    {0x80000u, 0x8000Eu, 2, {0x90, 0xFF}},
};

bool IsDetectProbeRead(uint32_t offset) {
    for (const auto& r : kDetectProbeReads) {
        if (offset - r.first <= r.last - r.first) return true;
    }
    return false;
}

bool IsDetectProbeWrite(uint32_t offset, uint8_t value) {
    for (const auto& w : kDetectProbeWrites) {
        if (offset - w.first > w.last - w.first) continue;
        for (uint8_t i = 0; i < w.count; ++i) {
            if (w.values[i] == value) return true;
        }
    }
    return false;
}

}

HpF1252aCard::HpF1252aCard(CerfEmulator& emu) : PcmciaCard(emu), controller_(emu) {}
HpF1252aCard::~HpF1252aCard() { DetachDisplay(); }

void HpF1252aCard::OnInserted() {
    controller_.Attach();
    PcmciaSlot*    slot = slot_;
    const uint64_t id   = card_id_;
    display_id_ = emu_.Get<ExternalDisplays>().Attach(controller_, kDisplayName, 640u, 480u,
                                                      [slot, id] { slot->EjectCardIfResident(id); });
    last_w_ = 640u;
    last_h_ = 480u;
}

void HpF1252aCard::OnShutdown() { DetachDisplay(); }

void HpF1252aCard::DetachDisplay() {
    if (display_id_ == 0) return;
    emu_.Get<ExternalDisplays>().Detach(display_id_);
    display_id_ = 0;
}

void HpF1252aCard::PowerOn() {
    controller_.PowerOnReset();
#if CERF_DEV_MODE
    LOG(Periph, "[HpVga] PowerOn\n");
#endif
}
void HpF1252aCard::PowerOff() {
#if CERF_DEV_MODE
    LOG(Periph, "[HpVga] PowerOff\n");
#endif
}

void HpF1252aCard::SyncDisplayToMode() {
    uint32_t w, h, bpp;
    if (!controller_.CurrentMode(w, h, bpp)) return;
    if (w == last_w_ && h == last_h_) return;
    last_w_ = w;
    last_h_ = h;
    emu_.Get<ExternalDisplays>().Resize(display_id_, w, h);
    LOG(Lcd, "[HpVga] mode %ux%u %ubpp\n", w, h, bpp);
}

bool HpF1252aCard::InMagic(uint32_t offset, uint32_t bytes) const {
    return offset + bytes > kMagicBase && offset < kMagicBase + kMagicLen;
}

void HpF1252aCard::HaltMagic(const char* access, uint32_t offset) {
    emu_.Get<Fatal>().Die("HpF1252aCard: common %s at 0x%06X of the identity bytes, which answer only an even "
                          "byte read outside the linear window", access, offset);
}

void HpF1252aCard::RequireOutsideMagic(const char* access, uint32_t offset, uint32_t bytes) {
    if (InMagic(offset, bytes)) HaltMagic(access, offset);
}

bool HpF1252aCard::LinearByte(uint32_t offset, uint32_t& at) const {
    return offset >= kRegEnd && controller_.LinearOffset(offset, at);
}

bool HpF1252aCard::LinearWord(uint32_t offset, uint32_t& at) const {
    return LinearByte(offset, at) && at + 1u < VgaController::kVramBytes;
}

uint8_t HpF1252aCard::ReadAttribute8(uint32_t offset) {
    if (offset < kCisSize * 2u) return kCisData[offset / 2u];
    if (InMagic(offset, 1u)) return kMagic[offset - kMagicBase];
    HaltUnmapped("attribute", "read", offset);
}

void HpF1252aCard::WriteAttribute8(uint32_t offset, uint8_t value) {
    emu_.Get<Fatal>().Die("HpF1252aCard: attribute write 0x%02X at 0x%06X", value, offset);
}

void HpF1252aCard::HaltUnmapped(const char* space, const char* access, uint32_t offset) {
    emu_.Get<Fatal>().Die("HpF1252aCard: %s %s at unmapped card offset 0x%06X", space, access,
                          offset);
}

uint8_t HpF1252aCard::ReadCommon8(uint32_t offset) {
    if (!InMagic(offset, 1u)) return ReadIo8(offset);
    uint32_t at = 0u;
    if (((offset - kMagicBase) & 1u) != 0u || LinearByte(offset, at)) HaltMagic("read", offset);
    return kMagic[offset - kMagicBase];
}

uint16_t HpF1252aCard::ReadCommon16(uint32_t offset) {
    RequireOutsideMagic("16-bit read", offset, 2u);
    return ReadIo16(offset);
}

void HpF1252aCard::WriteCommon8(uint32_t offset, uint8_t value) {
    RequireOutsideMagic("write", offset, 1u);
    WriteIo8(offset, value);
}

void HpF1252aCard::WriteCommon16(uint32_t offset, uint16_t value) {
    RequireOutsideMagic("16-bit write", offset, 2u);
    WriteIo16(offset, value);
}

uint8_t HpF1252aCard::ReadIo8(uint32_t offset) {
    uint32_t at = 0u;
    if (LinearByte(offset, at)) return controller_.Vram()[at];
    if (IsDetectProbeRead(offset)) return slot_->UndrivenDataLane();
    if (offset < kRegEnd) return controller_.ReadReg8(offset);
    HaltUnmapped("common", "read", offset);
}

void HpF1252aCard::WriteIo8(uint32_t offset, uint8_t value) {
    uint32_t at = 0u;
    if (LinearByte(offset, at)) {
        controller_.Vram()[at] = value;
        return;
    }
    if (IsDetectProbeWrite(offset, value)) return;
    if (offset >= kRegEnd) HaltUnmapped("common", "write", offset);
    controller_.WriteReg8(offset, value);
    SyncDisplayToMode();
}

uint16_t HpF1252aCard::ReadIo16(uint32_t offset) {
    uint32_t at = 0u;
    if (LinearWord(offset, at)) return cerf::le::U16(controller_.Vram(), at);
    const uint8_t bytes[2] = {ReadIo8(offset), ReadIo8(offset + 1u)};
    return cerf::le::U16(bytes);
}

void HpF1252aCard::SaveState(StateWriter& w) {
    controller_.SaveState(w);
}

void HpF1252aCard::RestoreState(StateReader& r) {
    controller_.RestoreState(r);
    last_w_ = 0;
    last_h_ = 0;
    SyncDisplayToMode();
}

void HpF1252aCard::WriteIo16(uint32_t offset, uint16_t value) {
    uint32_t at = 0u;
    if (LinearWord(offset, at)) {
        cerf::le::Put16(controller_.Vram() + at, value);
        return;
    }
    uint8_t bytes[2];
    cerf::le::Put16(bytes, value);
    WriteIo8(offset, bytes[0]);
    WriteIo8(offset + 1u, bytes[1]);
}

}
