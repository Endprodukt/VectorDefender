// The registers, flags and memory the translated program (src/gen) works on.
//
// The program is Defender's own, translated instruction by instruction from
// its 6809 source (tools/translate/asm2cpp.py); this header gives those
// statements their meaning: the 6809's arithmetic with its flags (after the
// Star Wars port's m6809.c), its stacks, and the Defender board's memory
// map (Machine, machine.cpp).
#pragma once

#include <cstdint>

namespace df {

enum : uint8_t
{
    CC_C = 0x01, CC_V = 0x02, CC_Z = 0x04, CC_N = 0x08,
    CC_I = 0x10, CC_H = 0x20, CC_F = 0x40, CC_E = 0x80
};

struct Cpu
{
    uint8_t a{0}, b{0}, dp{0}, cc{CC_I | CC_F};
    uint16_t x{0}, y{0}, u{0}, s{0}, pc{0};
    int64_t cyc{0};   // cycles run since the start
};

// One 6821 PIA: ports with their direction registers, control registers,
// the C1 lines (inputs). C2 is not used as an input on this board.
struct Pia
{
    uint8_t ddra{0}, ora{0}, cra{0}, ddrb{0}, orb{0}, crb{0};
    uint8_t inA{0}, inB{0};
    uint8_t ca1{0}, cb1{0};

    bool irqA() const { return (cra & 0x80) && (cra & 0x01); }
    bool irqB() const { return (crb & 0x80) && (crb & 0x01); }
    uint8_t outB() const { return static_cast<uint8_t>((orb & ddrb) | (~ddrb & 0xFF)); }
    uint8_t outA() const { return static_cast<uint8_t>((ora & ddra) | (~ddra & 0xFF)); }
    void setCA1(uint8_t level)
    {
        const bool rising = (cra & 0x02) != 0;
        if(level != ca1 && (level ? rising : !rising)) cra |= 0x80;
        ca1 = level;
    }
    void setCB1(uint8_t level)
    {
        const bool rising = (crb & 0x02) != 0;
        if(level != cb1 && (level ? rising : !rising)) crb |= 0x80;
        cb1 = level;
    }
};

class Machine;

// The memory regions of the program, each one translated function.
void run_fixed(Machine& m);
void run_bank1(Machine& m);
void run_bank2(Machine& m);
void run_bank3(Machine& m);
void run_bank7(Machine& m);

class Machine
{
public:
    Cpu c;
    int64_t limit{0};        // the translated code returns at an instruction start past this
    uint8_t bank{0};         // MAPC (D000): 0 = I/O at C000-CFFF, 1..7 the ROM banks

    uint8_t ram[0xC000]{};   // 0000-97FF the picture, 9800-BFFF the game's RAM
    uint8_t palette[16]{};   // BBGGGRRR
    uint8_t cmos[256]{};     // read with the top nibble set
    uint8_t flip{0};
    Pia pia[2];              // [0] at CC00: switches, sound, IRQ; [1] at CC04: controls

    int64_t frameStart{0};   // cycle the picture began at

    // hooks: the vector side's look at the game (Machine::hook)
    void (*onHook)(void* user, Machine& m, uint16_t pc){nullptr};
    void* hookUser{nullptr};
    // sound: what the game sends the sound board, with the cycle
    void (*onSound)(void* user, int64_t cycle, uint8_t value){nullptr};
    void* soundUser{nullptr};
    bool cmosDirty{false};
    uint16_t badPc{0};
    bool broken{false};

    void reset();
    // Run until cycle t, taking interrupts as the board does.
    void runUntil(int64_t t);

    bool irq() const { return pia[0].irqA() || pia[0].irqB(); }
    void hook(uint16_t pc);
    bool started{false};     // the game's SINIT has run (after the power-on tests)
    void bad(uint16_t pc);
    void sync();
    void cwai(uint16_t pc);

    uint8_t rd(uint16_t a)
    {
        if(a < 0xC000) return ram[a];
        if(a >= 0xD000) return romFixed[a - 0xD000];
        if(bank == 0) return ioRead(a);
        return banks[bank & 15][a - 0xC000];
    }
    void wr(uint16_t a, uint8_t v)
    {
        if(a < 0xC000) ram[a] = v;
        else if(a < 0xD000) { if(bank == 0) ioWrite(a, v); }
        else if(a < 0xE000) bank = v & 0x0F;   // MAPC; D000-FFFF is ROM
    }
    uint16_t rd16(uint16_t a) { return static_cast<uint16_t>((rd(a) << 8) | rd(static_cast<uint16_t>(a + 1))); }
    void wr16(uint16_t a, uint16_t v)
    {
        wr(a, static_cast<uint8_t>(v >> 8));
        wr(static_cast<uint16_t>(a + 1), static_cast<uint8_t>(v));
    }

    // The memory as the game sees it, without touching I/O (for the vector side).
    uint8_t peek(uint16_t a) const;
    uint8_t peekBank(int b, uint16_t a) const;
    uint16_t peek16(uint16_t a) const { return static_cast<uint16_t>((peek(a) << 8) | peek(static_cast<uint16_t>(a + 1))); }
    int line() const;        // the line the beam is on, 0..259

private:
    uint8_t ioRead(uint16_t a);
    void ioWrite(uint16_t a, uint8_t v);
    uint8_t piaRead(int n, int reg);
    void piaWrite(int n, int reg, uint8_t v);
    void dispatch();
    void takeIrq();

    static const uint8_t* const romFixed;
    static const uint8_t* const banks[16];
};

// --- what the translated statements use ------------------------------------

inline uint16_t getD(const Cpu& c) { return static_cast<uint16_t>((c.a << 8) | c.b); }
inline void setD(Cpu& c, uint16_t v)
{
    c.a = static_cast<uint8_t>(v >> 8);
    c.b = static_cast<uint8_t>(v);
}
inline void setZ(Cpu& c, bool z) { c.cc = static_cast<uint8_t>((c.cc & ~CC_Z) | (z ? CC_Z : 0)); }

inline void nz8(Cpu& c, uint8_t v)
{
    c.cc = static_cast<uint8_t>((c.cc & ~(CC_N | CC_Z)) | (v & 0x80 ? CC_N : 0) | (v ? 0 : CC_Z));
}
inline void nz16(Cpu& c, uint16_t v)
{
    c.cc = static_cast<uint8_t>((c.cc & ~(CC_N | CC_Z)) | (v & 0x8000 ? CC_N : 0) | (v ? 0 : CC_Z));
}
inline void logic8(Cpu& c, uint8_t v)
{
    nz8(c, v);
    c.cc &= static_cast<uint8_t>(~CC_V);
}
inline void logic16(Cpu& c, uint16_t v)
{
    nz16(c, v);
    c.cc &= static_cast<uint8_t>(~CC_V);
}
inline uint16_t ld16(Cpu& c, uint16_t v)
{
    logic16(c, v);
    return v;
}

inline uint8_t add8(Cpu& c, uint8_t a, uint8_t b, unsigned carry)
{
    const unsigned r = static_cast<unsigned>(a) + b + carry;
    c.cc &= static_cast<uint8_t>(~(CC_H | CC_N | CC_Z | CC_V | CC_C));
    if((a ^ b ^ r) & 0x10) c.cc |= CC_H;
    if(r & 0x100) c.cc |= CC_C;
    if((a ^ r) & (b ^ r) & 0x80) c.cc |= CC_V;
    if(r & 0x80) c.cc |= CC_N;
    if(!(r & 0xFF)) c.cc |= CC_Z;
    return static_cast<uint8_t>(r);
}
inline uint8_t sub8(Cpu& c, uint8_t a, uint8_t b, unsigned carry)
{
    const unsigned r = static_cast<unsigned>(a) - b - carry;
    c.cc &= static_cast<uint8_t>(~(CC_N | CC_Z | CC_V | CC_C));
    if(r & 0x100) c.cc |= CC_C;
    if((a ^ b) & (a ^ r) & 0x80) c.cc |= CC_V;
    if(r & 0x80) c.cc |= CC_N;
    if(!(r & 0xFF)) c.cc |= CC_Z;
    return static_cast<uint8_t>(r);
}
inline uint16_t add16(Cpu& c, uint16_t a, uint16_t b)
{
    const uint32_t r = static_cast<uint32_t>(a) + b;
    c.cc &= static_cast<uint8_t>(~(CC_N | CC_Z | CC_V | CC_C));
    if(r & 0x10000) c.cc |= CC_C;
    if((a ^ r) & (b ^ r) & 0x8000) c.cc |= CC_V;
    if(r & 0x8000) c.cc |= CC_N;
    if(!(r & 0xFFFF)) c.cc |= CC_Z;
    return static_cast<uint16_t>(r);
}
inline uint16_t sub16(Cpu& c, uint16_t a, uint16_t b)
{
    const uint32_t r = static_cast<uint32_t>(a) - b;
    c.cc &= static_cast<uint8_t>(~(CC_N | CC_Z | CC_V | CC_C));
    if(r & 0x10000) c.cc |= CC_C;
    if((a ^ b) & (a ^ r) & 0x8000) c.cc |= CC_V;
    if(r & 0x8000) c.cc |= CC_N;
    if(!(r & 0xFFFF)) c.cc |= CC_Z;
    return static_cast<uint16_t>(r);
}

inline uint8_t neg8(Cpu& c, uint8_t v) { return sub8(c, 0, v, 0); }
inline uint8_t com8(Cpu& c, uint8_t v)
{
    const uint8_t r = static_cast<uint8_t>(~v);
    logic8(c, r);
    c.cc |= CC_C;
    return r;
}
inline uint8_t lsr8(Cpu& c, uint8_t v)
{
    const uint8_t r = v >> 1;
    c.cc = static_cast<uint8_t>((c.cc & ~(CC_N | CC_Z | CC_C)) | (v & 1) | (r ? 0 : CC_Z));
    return r;
}
inline uint8_t ror8(Cpu& c, uint8_t v)
{
    const uint8_t r = static_cast<uint8_t>((v >> 1) | ((c.cc & CC_C) << 7));
    nz8(c, r);
    c.cc = static_cast<uint8_t>((c.cc & ~CC_C) | (v & 1));
    return r;
}
inline uint8_t asr8(Cpu& c, uint8_t v)
{
    const uint8_t r = static_cast<uint8_t>((v >> 1) | (v & 0x80));
    nz8(c, r);
    c.cc = static_cast<uint8_t>((c.cc & ~CC_C) | (v & 1));
    return r;
}
inline uint8_t asl8(Cpu& c, uint8_t v)
{
    const uint8_t r = static_cast<uint8_t>(v << 1);
    nz8(c, r);
    c.cc = static_cast<uint8_t>((c.cc & ~(CC_C | CC_V)) | (v >> 7) | (((v ^ r) & 0x80) ? CC_V : 0));
    return r;
}
inline uint8_t rol8(Cpu& c, uint8_t v)
{
    const uint8_t r = static_cast<uint8_t>((v << 1) | (c.cc & CC_C));
    nz8(c, r);
    c.cc = static_cast<uint8_t>((c.cc & ~(CC_C | CC_V)) | (v >> 7) | (((v ^ r) & 0x80) ? CC_V : 0));
    return r;
}
inline uint8_t dec8(Cpu& c, uint8_t v)
{
    const uint8_t r = static_cast<uint8_t>(v - 1);
    nz8(c, r);
    c.cc = static_cast<uint8_t>((c.cc & ~CC_V) | (v == 0x80 ? CC_V : 0));
    return r;
}
inline uint8_t inc8(Cpu& c, uint8_t v)
{
    const uint8_t r = static_cast<uint8_t>(v + 1);
    nz8(c, r);
    c.cc = static_cast<uint8_t>((c.cc & ~CC_V) | (v == 0x7F ? CC_V : 0));
    return r;
}
inline void tst8(Cpu& c, uint8_t v) { logic8(c, v); }
inline uint8_t clr8(Cpu& c)
{
    c.cc = static_cast<uint8_t>((c.cc & ~(CC_N | CC_V | CC_C)) | CC_Z);
    return 0;
}

inline void daa(Cpu& c)
{
    unsigned corr = 0;
    const unsigned lo = c.a & 0x0F, hi = c.a >> 4;
    if((c.cc & CC_H) || lo > 9) corr |= 0x06;
    if((c.cc & CC_C) || hi > 9 || (hi > 8 && lo > 9)) corr |= 0x60;
    const unsigned r = c.a + corr;
    c.a = static_cast<uint8_t>(r);
    nz8(c, c.a);
    if(r & 0x100) c.cc |= CC_C;
}
inline void sex(Cpu& c)
{
    c.a = (c.b & 0x80) ? 0xFF : 0x00;
    nz16(c, getD(c));
    c.cc &= static_cast<uint8_t>(~CC_V);
}
inline void mul(Cpu& c)
{
    const uint16_t r = static_cast<uint16_t>(c.a * c.b);
    setD(c, r);
    c.cc = static_cast<uint8_t>((c.cc & ~(CC_Z | CC_C)) | (r ? 0 : CC_Z) | ((r & 0x80) ? CC_C : 0));
}
inline bool ge(const Cpu& c) { return !(c.cc & CC_N) == !(c.cc & CC_V); }
inline bool gt(const Cpu& c) { return !(c.cc & CC_Z) && ge(c); }

inline void pushs8(Machine& m, Cpu& c, uint8_t v) { m.wr(--c.s, v); }
inline void pushu8(Machine& m, Cpu& c, uint8_t v) { m.wr(--c.u, v); }
inline void pushs16(Machine& m, Cpu& c, uint16_t v)
{
    pushs8(m, c, static_cast<uint8_t>(v));
    pushs8(m, c, static_cast<uint8_t>(v >> 8));
}
inline void pushu16(Machine& m, Cpu& c, uint16_t v)
{
    pushu8(m, c, static_cast<uint8_t>(v));
    pushu8(m, c, static_cast<uint8_t>(v >> 8));
}
inline uint8_t pulls8(Machine& m, Cpu& c) { return m.rd(c.s++); }
inline uint8_t pullu8(Machine& m, Cpu& c) { return m.rd(c.u++); }
inline uint16_t pulls16(Machine& m, Cpu& c)
{
    const uint8_t h = pulls8(m, c);
    return static_cast<uint16_t>((h << 8) | pulls8(m, c));
}
inline uint16_t pullu16(Machine& m, Cpu& c)
{
    const uint8_t h = pullu8(m, c);
    return static_cast<uint16_t>((h << 8) | pullu8(m, c));
}
inline void push_all(Machine& m, Cpu& c, uint16_t pc)
{
    pushs16(m, c, pc);
    pushs16(m, c, c.u);
    pushs16(m, c, c.y);
    pushs16(m, c, c.x);
    pushs8(m, c, c.dp);
    pushs8(m, c, c.b);
    pushs8(m, c, c.a);
    pushs8(m, c, c.cc);
}

} // namespace df
