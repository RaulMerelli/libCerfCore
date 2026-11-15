#include "pcmcia_card_identity.h"

#include <cerfcore/core/utf16.h>
#include <cerfcore/state/state_stream.h>

#include <cstdint>

namespace cerfcore {

void PcmciaCardIdentity::Save(StateWriter& w, const std::string& prefix) const {
    w.Write<uint32_t>((prefix + "id_count").c_str(), static_cast<uint32_t>(id.size()));
    if (!id.empty()) w.WriteBytes((prefix + "id").c_str(), id.data(), id.size());
    const std::u16string units = Utf8ToUtf16(binding);
    w.Write<uint32_t>((prefix + "binding_count").c_str(), static_cast<uint32_t>(units.size()));
    if (!units.empty()) w.WriteBytes((prefix + "binding").c_str(), units.data(), units.size() * sizeof(char16_t));
}

PcmciaCardIdentity PcmciaCardIdentity::Restore(StateReader& r, const std::string& prefix) {
    PcmciaCardIdentity identity;
    uint32_t           idlen = 0;
    r.Read((prefix + "id_count").c_str(), idlen);
    identity.id.assign(static_cast<size_t>(idlen), '\0');
    if (idlen) r.ReadBytes((prefix + "id").c_str(), &identity.id[0], idlen);
    uint32_t blen = 0;
    r.Read((prefix + "binding_count").c_str(), blen);
    std::u16string units(static_cast<size_t>(blen), u'\0');
    if (blen) r.ReadBytes((prefix + "binding").c_str(), &units[0], blen * sizeof(char16_t));
    identity.binding = Utf16ToUtf8(units);
    return identity;
}

}
