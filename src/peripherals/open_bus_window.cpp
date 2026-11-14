#include "open_bus_window.h"

#include <cerfcore/core/cerf_emulator.h>
#include "peripheral_dispatcher.h"

namespace cerfcore {

bool OpenBusWindow::ShouldRegister() {
    const BoardContext& bd = emu_.Get<BoardContext>();
    return bd.GetBoardFamilyId() == WindowBoardFamilyId() && WindowOnBoard(bd);
}

bool OpenBusWindow::WindowOnBoard(const BoardContext&) const { return true; }

bool OpenBusWindow::Floats(uint32_t, uint32_t bits, bool) const { return bits <= 32u; }

void OpenBusWindow::Require(const char* op, uint32_t addr, uint32_t bits, bool write, uint64_t value) const {
    if (!Floats(addr, bits, write)) HaltUnsupportedAccess(op, addr, value);
}

void OpenBusWindow::OnReady() {
    emu_.Get<PeripheralDispatcher>().Register(this);
}

uint8_t OpenBusWindow::ReadByte(uint32_t addr) {
    Require("ReadByte", addr, 8u, false, 0u);
    return bus_.Read8(WindowTag(), addr);
}

uint16_t OpenBusWindow::ReadHalf(uint32_t addr) {
    Require("ReadHalf", addr, 16u, false, 0u);
    return bus_.Read16(WindowTag(), addr);
}

uint32_t OpenBusWindow::ReadWord(uint32_t addr) {
    Require("ReadWord", addr, 32u, false, 0u);
    return bus_.Read32(WindowTag(), addr);
}

uint64_t OpenBusWindow::ReadDword(uint32_t addr) {
    Require("ReadDword", addr, 64u, false, 0u);
    return bus_.Read64(WindowTag(), addr);
}

void OpenBusWindow::WriteByte(uint32_t addr, uint8_t value) {
    Require("WriteByte", addr, 8u, true, value);
    bus_.Write(WindowTag(), 8u, addr);
}

void OpenBusWindow::WriteHalf(uint32_t addr, uint16_t value) {
    Require("WriteHalf", addr, 16u, true, value);
    bus_.Write(WindowTag(), 16u, addr);
}

void OpenBusWindow::WriteWord(uint32_t addr, uint32_t value) {
    Require("WriteWord", addr, 32u, true, value);
    bus_.Write(WindowTag(), 32u, addr);
}

void OpenBusWindow::WriteDword(uint32_t addr, uint64_t value) {
    Require("WriteDword", addr, 64u, true, value);
    bus_.Write(WindowTag(), 64u, addr);
}

}
