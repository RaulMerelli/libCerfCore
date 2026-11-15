#pragma once

#include <cstdint>

namespace cerfcore {

struct TypedKey {
    uint8_t vk    = 0;
    bool    shift = false;
};

bool KeyForCharacter(char32_t character, TypedKey& out);

}
