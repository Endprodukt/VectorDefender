// The Defender board around the translated program: see cpu.hpp.
//
// Memory map and timing as MAME's williams.cpp describes the Red Label
// board: 1 MHz, 260 lines of 64 cycles; the first PIA's CB1 follows line
// address bit 5 (set every 32 lines, so the game's IRQ comes on lines 0, 64,
// 128 and 192), its CA1 is high from line 240; the video counter at C800
// reads the line with the low two bits clear (FC past line 255).
#include "game/cpu.hpp"

#include "gen/defender_syms.hpp"

#include <cstdio>

namespace df {

namespace {
#include "gen/defender_data.inc"

const uint8_t NO_ROM[0x1000] = {};
} // namespace

const uint8_t* const Machine::romFixed = ROM_FIXED;
const uint8_t* const Machine::banks[16] = {
    NO_ROM, ROM_BANK1, ROM_BANK2, ROM_BANK3, NO_ROM, NO_ROM, NO_ROM, ROM_BANK7,
    NO_ROM, NO_ROM, NO_ROM, NO_ROM, NO_ROM, NO_ROM, NO_ROM, NO_ROM};

void Machine::reset()
{
    const int64_t now = c.cyc;
    c = Cpu{};
    c.cyc = now;
    bank = 0;
    for(Pia& p : pia)
    {
        const uint8_t a = p.inA, b = p.inB;
        p = Pia{};
        p.inA = a;
        p.inB = b;
    }
    c.pc = rd16(0xFFFE);
    broken = false;
    started = false;
}

int Machine::line() const
{
    return static_cast<int>((c.cyc - frameStart) / 64);
}

uint8_t Machine::peekBank(int b, uint16_t a) const
{
    if(a >= 0xC000 && a < 0xD000) return banks[b & 15][a - 0xC000];
    return peek(a);
}

uint8_t Machine::peek(uint16_t a) const
{
    if(a < 0xC000) return ram[a];
    if(a >= 0xD000) return romFixed[a - 0xD000];
    if(bank != 0) return banks[bank & 15][a - 0xC000];
    if(a >= 0xC400 && a < 0xC800) return cmos[a & 0xFF];
    return 0;
}

uint8_t Machine::piaRead(int n, int reg)
{
    Pia& p = pia[n];
    switch(reg)
    {
        case 0:
            if(!(p.cra & 0x04)) return p.ddra;
            p.cra &= 0x3F;   // reading the port clears its interrupt flags
            return static_cast<uint8_t>((p.ora & p.ddra) | (p.inA & ~p.ddra));
        case 1: return p.cra;
        case 2:
            if(!(p.crb & 0x04)) return p.ddrb;
            p.crb &= 0x3F;
            return static_cast<uint8_t>((p.orb & p.ddrb) | (p.inB & ~p.ddrb));
        default: return p.crb;
    }
}

void Machine::piaWrite(int n, int reg, uint8_t v)
{
    Pia& p = pia[n];
    const uint8_t soundBefore = p.outB();
    switch(reg)
    {
        case 0:
            if(p.cra & 0x04) p.ora = v;
            else p.ddra = v;
            break;
        case 1: p.cra = static_cast<uint8_t>((p.cra & 0xC0) | (v & 0x3F)); break;
        case 2:
            if(p.crb & 0x04) p.orb = v;
            else p.ddrb = v;
            break;
        default: p.crb = static_cast<uint8_t>((p.crb & 0xC0) | (v & 0x3F)); break;
    }
    // The first PIA's port B goes to the sound board (MAME: snd_cmd_w on
    // every write of the port).
    if(n == 0 && (reg == 2) && onSound) onSound(soundUser, c.cyc, p.outB());
    (void)soundBefore;
    // An interrupt enabled just now with its flag already set: take it at
    // the next instruction.
    if(irq() && !(c.cc & CC_I)) limit = 0;
}

uint8_t Machine::ioRead(uint16_t a)
{
    if(a < 0xC400) return 0;   // colour registers, video control: write only
    if(a < 0xC800) return cmos[a & 0xFF];
    if(a < 0xCC00)
    {
        const int l = line();
        return l < 256 ? static_cast<uint8_t>(l & 0xFC) : 0xFC;
    }
    const int low = a & 0x1F;
    if(low < 4) return piaRead(0, low & 3);
    if(low < 8) return piaRead(1, low & 3);
    return 0;
}

void Machine::ioWrite(uint16_t a, uint8_t v)
{
    if(a < 0xC400)
    {
        const int low = a & 0x1F;
        if(low < 0x10) palette[low] = v;
        else flip = v & 1;   // C3FF, the watchdog, lands here too
        return;
    }
    if(a < 0xC800)
    {
        const uint8_t nv = static_cast<uint8_t>(v | 0xF0);
        if(cmos[a & 0xFF] != nv) cmosDirty = true;
        cmos[a & 0xFF] = nv;
        return;
    }
    if(a < 0xCC00) return;
    const int low = a & 0x1F;
    if(low < 4) piaWrite(0, low & 3, v);
    else if(low < 8) piaWrite(1, low & 3, v);
}

void Machine::hook(uint16_t pc)
{
    if(pc == sym::SINIT)
    {
        started = true;
        return;
    }
    if(onHook) onHook(hookUser, *this, pc);
}

void Machine::bad(uint16_t pc)
{
    if(!broken) std::fprintf(stderr, "defender: no instruction at %04X (bank %d)\n", pc, bank);
    badPc = pc;
    broken = true;
    c.cyc = limit > c.cyc ? limit : c.cyc + 1;
}

void Machine::sync() { bad(c.pc); }
void Machine::cwai(uint16_t pc) { bad(pc); }

void Machine::takeIrq()
{
    c.cc |= CC_E;
    push_all(*this, c, c.pc);
    c.cc |= CC_I;
    c.pc = rd16(0xFFF8);
    c.cyc += 19;
}

// Run the region the program is in until it leaves it or the limit is hit.
void Machine::dispatch()
{
    const uint16_t pc = c.pc;
    if(pc >= 0xD000) { run_fixed(*this); return; }
    if(pc >= 0xC000)
    {
        switch(bank)
        {
            case 1: run_bank1(*this); return;
            case 2: run_bank2(*this); return;
            case 3: run_bank3(*this); return;
            case 7: run_bank7(*this); return;
            default: bad(pc); return;
        }
    }
    // RAM: only the IRQ hook (IRQHK, A024) runs from there, a JMP the game
    // writes at start (JMP IRQ, or IRQB for the flipped screen).
    if(rd(pc) == 0x7E)
    {
        c.cyc += 4;
        c.pc = rd16(static_cast<uint16_t>(pc + 1));
        return;
    }
    bad(pc);
}

void Machine::runUntil(int64_t t)
{
    while(c.cyc < t && !broken)
    {
        if(irq() && !(c.cc & CC_I)) takeIrq();
        limit = t;
        const uint16_t pc = c.pc;
        const uint8_t bk = bank;
        const int64_t before = c.cyc;
        dispatch();
        // Back without running out of time, still at the same place: the
        // translation has no instruction there.
        if(limit != 0 && c.cyc < t && c.pc == pc && bank == bk && c.cyc == before) bad(pc);
    }
}

} // namespace df
