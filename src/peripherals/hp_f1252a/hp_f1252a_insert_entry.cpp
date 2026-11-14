#include "hp_f1252a_insert_entry.h"

#include "hp_f1252a_card.h"
#include <cerfcore/core/cerf_emulator.h>

#include <memory>

namespace cerfcore {

REGISTER_SERVICE(HpF1252aInsertEntry);

SlotEntry HpF1252aInsertEntry::BuildInsertEntry(PcmciaCardCatalog::CardInserter inserter) {
    SlotEntry entry;
    entry.insert = [this, inserter] {
        inserter(std::make_unique<HpF1252aCard>(emu_));
    };
    entry.details = {
        WidgetControl{InfoControl{"Only HP devices include the driver"}},
    };
    return entry;
}

}
