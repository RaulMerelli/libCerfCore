#include <cerfcore/host/character_keys.h>

#include <cerfcore/host/virtual_keys.h>

namespace cerfcore {

namespace {

struct CharacterKey {
    char    character;
    uint8_t vk;
    bool    shift;
};

constexpr CharacterKey kSymbolKeys[] = {
    {'`', 0xC0, false}, {'-', 0xBD, false}, {'=', 0xBB, false}, {'[', 0xDB, false},  {']', 0xDD, false},
    {'\\', 0xDC, false}, {';', 0xBA, false}, {'\'', 0xDE, false}, {',', 0xBC, false}, {'.', 0xBE, false},
    {'/', 0xBF, false}, {'~', 0xC0, true},  {'!', 0x31, true},  {'@', 0x32, true},   {'#', 0x33, true},
    {'$', 0x34, true},  {'%', 0x35, true},  {'^', 0x36, true},  {'&', 0x37, true},   {'*', 0x38, true},
    {'(', 0x39, true},  {')', 0x30, true},  {'_', 0xBD, true},  {'+', 0xBB, true},   {'{', 0xDB, true},
    {'}', 0xDD, true},  {'|', 0xDC, true},  {':', 0xBA, true},  {'"', 0xDE, true},   {'<', 0xBC, true},
    {'>', 0xBE, true},  {'?', 0xBF, true},  {' ', kVkSpace, false}, {'\n', kVkReturn, false},
    {'\t', kVkTab, false},
};

}

bool KeyForCharacter(char32_t character, TypedKey& out) {
    if (character >= U'a' && character <= U'z') {
        out = {static_cast<uint8_t>(character - U'a' + U'A'), false};
        return true;
    }
    if (character >= U'A' && character <= U'Z') {
        out = {static_cast<uint8_t>(character), true};
        return true;
    }
    if (character >= U'0' && character <= U'9') {
        out = {static_cast<uint8_t>(character), false};
        return true;
    }
    for (const CharacterKey& key : kSymbolKeys) {
        if (static_cast<char32_t>(static_cast<unsigned char>(key.character)) != character) continue;
        out = {key.vk, key.shift};
        return true;
    }
    return false;
}

}
