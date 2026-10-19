// The game's own settings in its CMOS: see adjust.hpp.
#include "adjust.hpp"

#include "gen/defender_syms.hpp"

namespace vp {

const char* const auditNames[7] = {
    "COINS LEFT", "COINS CENTER", "COINS RIGHT", "TOTAL PAID", "SHIPS WON", "TOTAL TIME", "TOTAL SHIPS"};

// The presets of the original's coinage table (CSELCT), as its comments name them.
const char* const coinageNames[9] = {
    "AS SET", "50C, 3 FOR $1", "GERMANY", "25C", "FRANCE", "50C", "SET 6", "SET 7", "SET 8"};

namespace {

using df::sym::CMOS;

int cell(uint16_t addr) { return addr - CMOS; }

// A byte (two BCD digits) at a CMOS address: two cells, high digit first.
int readByte(const df::Board& b, uint16_t addr)
{
    const int i = cell(addr);
    return ((b.m.cmos[i] & 0x0F) << 4) | (b.m.cmos[i + 1] & 0x0F);
}

void writeByte(df::Board& b, uint16_t addr, int v)
{
    const int i = cell(addr);
    const uint8_t hi = static_cast<uint8_t>(0xF0 | ((v >> 4) & 0x0F)), lo = static_cast<uint8_t>(0xF0 | (v & 0x0F));
    if(b.m.cmos[i] != hi || b.m.cmos[i + 1] != lo) b.m.cmosDirty = true;
    b.m.cmos[i] = hi;
    b.m.cmos[i + 1] = lo;
}

int fromBcd(int v) { return (v >> 4) * 10 + (v & 0x0F); }
int toBcd(int n) { return ((n / 10 % 10) << 4) | (n % 10); }

uint8_t romByte(const df::Board& b, uint16_t addr) { return b.m.peekBank(3, addr); }

} // namespace

GameAdjust readAdjust(const df::Board& b)
{
    using namespace df::sym;
    GameAdjust a;
    // REPLAY: four digits, in hundreds
    a.bonusShip = (fromBcd(readByte(b, REPLAY)) * 100 + fromBcd(readByte(b, static_cast<uint16_t>(REPLAY + 2)))) * 100;
    a.ships = fromBcd(readByte(b, NSHIP));
    a.coinage = fromBcd(readByte(b, COINSL));
    if(a.coinage > 8) a.coinage = 0;
    a.freePlay = readByte(b, FREEPL) != 0;
    a.difficulty = fromBcd(readByte(b, GA1));
    a.ceiling = fromBcd(readByte(b, static_cast<uint16_t>(GA1 + 2)));
    a.restoreWave = fromBcd(readByte(b, static_cast<uint16_t>(GA1 + 6)));
    for(int k = 0; k < 7; ++k)
    {
        const uint16_t at = static_cast<uint16_t>(SLOT1 + 4 * k);
        a.audits[k] = fromBcd(readByte(b, at)) * 100 + fromBcd(readByte(b, static_cast<uint16_t>(at + 2)));
    }
    return a;
}

bool writeAdjust(df::Board& b, const GameAdjust& a)
{
    using namespace df::sym;
    bool restart = false;
    if(a.factory)
    {
        // CMINIT: everything from the high scores on as the ROM has it
        for(int k = 0; k < ROMS_DEFSIZ; ++k)
            writeByte(b, static_cast<uint16_t>(CRHSTD + 2 * k), romByte(b, static_cast<uint16_t>(ROMS_DEFALT + k)));
        return true;
    }
    if(a.resetScores)
    {
        // RHSTD: the all-time table back to the ROM's (8 x 6 bytes); today's
        // follows at the next power on
        for(int k = 0; k < 8 * 6; ++k)
            writeByte(b, static_cast<uint16_t>(CRHSTD + 2 * k), romByte(b, static_cast<uint16_t>(ROMS_DEFALT + k)));
        restart = true;
    }
    if(a.resetAudits)
        for(int k = 0; k < 14; ++k) writeByte(b, static_cast<uint16_t>(SLOT1 + 2 * k), 0);   // CLRAUD
    const int hundreds = a.bonusShip / 100;
    writeByte(b, REPLAY, toBcd(hundreds / 100));
    writeByte(b, static_cast<uint16_t>(REPLAY + 2), toBcd(hundreds % 100));
    writeByte(b, NSHIP, toBcd(a.ships));
    writeByte(b, FREEPL, a.freePlay ? 1 : 0);
    writeByte(b, GA1, toBcd(a.difficulty));
    writeByte(b, static_cast<uint16_t>(GA1 + 2), toBcd(a.ceiling));
    writeByte(b, static_cast<uint16_t>(GA1 + 6), toBcd(a.restoreWave));
    if(a.coinage != fromBcd(readByte(b, COINSL)))
    {
        // COININ: the mode, and with a preset its six values into SLOT1M...
        writeByte(b, COINSL, toBcd(a.coinage));
        if(a.coinage > 0)
            for(int k = 0; k < 6; ++k)
                writeByte(b, static_cast<uint16_t>(SLOT1M + 2 * k),
                          romByte(b, static_cast<uint16_t>(ROMS_CSELCT + (a.coinage - 1) * 6 + k)));
    }
    return restart;
}

} // namespace vp
