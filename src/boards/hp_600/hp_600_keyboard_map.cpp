#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/keyboard_map.h>
#include "../jornada/jornada_keyboard_layers.h"
#include "hp_600_family_id.h"
#include "hp_600_keyboard_layout.h"

#include <cstdint>
#include <vector>

namespace cerfcore {

namespace {

constexpr uint8_t kVkShift       = 0x10u;
constexpr uint8_t kVkControl     = 0x11u;
constexpr uint8_t kVkMenu        = 0x12u;
constexpr uint8_t kVkLeftShift   = 0xA0u;
constexpr uint8_t kVkLeftControl = 0xA2u;
constexpr uint8_t kVkLeftMenu    = 0xA4u;
constexpr uint8_t kVkRightMenu   = 0xA5u;
constexpr uint8_t kCellAltGr     = 0x79u;

constexpr uint8_t kAltLayerId   = 2u;
constexpr uint8_t kAltGrLayerId = 3u;

struct GenericModifier {
    uint8_t vk;
    uint8_t sided_vk;
};

constexpr GenericModifier kGenericModifiers[] = {
    {kVkShift, kVkLeftShift},
    {kVkControl, kVkLeftControl},
    {kVkMenu, kVkLeftMenu},
};

constexpr jornada_kbd::LayerKey kContrastLayer[] = {
    {0xBCu, "Contrast -"},
    {0xBEu, "Contrast +"},
};

class Hp600KeyboardMap : public KeyboardMap {
public:
    using KeyboardMap::KeyboardMap;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp600;
    }

    void OnReady() override {
        const Hp600KeyboardLayout& layout = emu_.Get<Hp600KeyboardLayout>();
        for (uint32_t cell = 0; cell < kHp600KeyCells; ++cell) {
            const uint8_t vk = layout.CellVk()[cell];
            if (vk == 0u || !layout.Scanned(cell)) continue;
            if (vk == kCellAltGr) {
                bindings_.push_back({kVkRightMenu, cell, nullptr, 0u, kAltGrLayerId});
                continue;
            }
            bindings_.push_back({vk, cell, nullptr, 0u, vk == kVkLeftMenu ? kAltLayerId : uint8_t{0u}});
            for (const GenericModifier& m : kGenericModifiers) {
                if (m.sided_vk == vk) {
                    bindings_.push_back({m.vk, cell, nullptr, 0u, m.vk == kVkMenu ? kAltLayerId : uint8_t{0u}});
                }
            }
            for (const Hp600HostAlias& a : layout.HostAliases()) {
                if (a.guest_vk == vk) bindings_.push_back({a.host_vk, cell, a.label, 0u, 0u});
            }
        }
        for (const jornada_kbd::LayerKey& k : jornada_kbd::kAltLayer) AddLayerLabel(k, kAltLayerId);
        for (const jornada_kbd::LayerKey& k : kContrastLayer) AddLayerLabel(k, kAltLayerId);
        for (const jornada_kbd::LayerKey& k : layout.AltGrLayer()) AddLayerLabel(k, kAltGrLayerId);
    }

    const std::vector<KeyBinding>& Bindings() const override { return bindings_; }

private:
    void AddLayerLabel(const jornada_kbd::LayerKey& k, uint8_t layer) {
        bindings_.push_back({k.vk, 0u, k.label, layer, 0u});
    }

    std::vector<KeyBinding> bindings_;
};

}

REGISTER_SERVICE_AS(Hp600KeyboardMap, KeyboardMap);

}
