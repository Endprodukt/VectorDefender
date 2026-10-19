// The Defender sound board around its translated program (src/gen/
// sound_code.cpp, from VSNDRM1's source): a 6808 at 894.886 kHz with 128
// bytes of RAM, a 6821 PIA (port A the DAC, port B and CB1 the main board's
// sound number) and the program at F800-FFFF. The DAC's level, held between
// the program's writes, is averaged into samples.
#pragma once

#include "game/cpu.hpp"

#include <cstdint>
#include <vector>

namespace df {

constexpr double SOUND_CLOCK = 3579545.0 / 4.0;

struct SoundCpu
{
    uint8_t a{0}, b{0}, cc{0xC0 | CC_I};
    uint16_t x{0}, sp{0}, pc{0};
    int64_t cyc{0};
};

class SoundBoard
{
public:
    SoundCpu c;
    int64_t limit{0};
    bool wai{false};
    uint8_t ram[128]{};
    Pia pia;

    void reset();
    // What the main board's PIA puts on the sound lines (MAME's snd_cmd_w:
    // the top two lines pulled high, CB1 "not $FF").
    void command(uint8_t value);
    // Run to cycle t of the board's clock.
    void runUntil(int64_t t);
    // The sound since the last call, `n` samples spread over the cycles run
    // since then (the caller's rate), DC removed.
    void takeSamples(float* out, int n);

    bool irq() const { return pia.irqA() || pia.irqB(); }
    void bad(uint16_t pc);

    uint8_t rd(uint16_t a)
    {
        if(a < 0x0400) return ram[a & 0x7F];
        if((a & 0x7FFC) == 0x0400) return piaRead(a & 3);
        if(a >= 0xF800) return rom[a - 0xF800];
        return 0;
    }
    void wr(uint16_t a, uint8_t v)
    {
        if(a < 0x0400) ram[a & 0x7F] = v;
        else if((a & 0x7FFC) == 0x0400) piaWrite(a & 3, v);
    }
    void push8(uint8_t v) { wr(c.sp--, v); }
    uint8_t pull8() { return rd(++c.sp); }
    void push16(uint16_t v)
    {
        push8(static_cast<uint8_t>(v));
        push8(static_cast<uint8_t>(v >> 8));
    }
    uint16_t pull16()
    {
        const uint8_t h = pull8();
        return static_cast<uint16_t>((h << 8) | pull8());
    }
    uint16_t rd16(uint16_t a) { return static_cast<uint16_t>((rd(a) << 8) | rd(static_cast<uint16_t>(a + 1))); }
    void wr16(uint16_t a, uint16_t v)
    {
        wr(a, static_cast<uint8_t>(v >> 8));
        wr(static_cast<uint16_t>(a + 1), static_cast<uint8_t>(v));
    }

private:
    uint8_t piaRead(int reg);
    void piaWrite(int reg, uint8_t v);
    void dacChange();
    void takeIrq();

    static const uint8_t* const rom;
    uint8_t dac_{0x80};
    uint8_t spanLevel_{0x80};  // the DAC's level when the last takeSamples ended
    int64_t sumFrom_{0};       // the cycle it ended at
    std::vector<std::pair<int64_t, uint8_t>> changes_;   // DAC levels since then, with their cycle
    float hpIn_{0}, hpOut_{0};
};

void run_sound(SoundBoard& s);

// --- what the translated statements use (the 6800's own flags) -------------

inline void setZ(SoundCpu& c, bool z) { c.cc = static_cast<uint8_t>((c.cc & ~CC_Z) | (z ? CC_Z : 0)); }
inline void nz8(SoundCpu& c, uint8_t v)
{
    c.cc = static_cast<uint8_t>((c.cc & ~(CC_N | CC_Z)) | (v & 0x80 ? CC_N : 0) | (v ? 0 : CC_Z));
}
inline void logic8(SoundCpu& c, uint8_t v)
{
    nz8(c, v);
    c.cc &= static_cast<uint8_t>(~CC_V);
}
inline void logic16(SoundCpu& c, uint16_t v)
{
    c.cc = static_cast<uint8_t>((c.cc & ~(CC_N | CC_Z | CC_V)) | (v & 0x8000 ? CC_N : 0) | (v ? 0 : CC_Z));
}
inline uint16_t ld16(SoundCpu& c, uint16_t v)
{
    logic16(c, v);
    return v;
}
inline uint8_t add8(SoundCpu& c, uint8_t a, uint8_t b, unsigned carry)
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
inline uint8_t sub8(SoundCpu& c, uint8_t a, uint8_t b, unsigned carry)
{
    const unsigned r = static_cast<unsigned>(a) - b - carry;
    c.cc &= static_cast<uint8_t>(~(CC_N | CC_Z | CC_V | CC_C));
    if(r & 0x100) c.cc |= CC_C;
    if((a ^ b) & (a ^ r) & 0x80) c.cc |= CC_V;
    if(r & 0x80) c.cc |= CC_N;
    if(!(r & 0xFF)) c.cc |= CC_Z;
    return static_cast<uint8_t>(r);
}
// CPX: the 6800 compares X's high bytes for N and V, all 16 bits for Z;
// C is not touched.
inline void cpx(SoundCpu& c, uint16_t v)
{
    const uint16_t r = static_cast<uint16_t>(c.x - v);
    const uint8_t xh = static_cast<uint8_t>(c.x >> 8), vh = static_cast<uint8_t>(v >> 8);
    const uint8_t rh = static_cast<uint8_t>(xh - vh);
    c.cc &= static_cast<uint8_t>(~(CC_N | CC_Z | CC_V));
    if(rh & 0x80) c.cc |= CC_N;
    if(!r) c.cc |= CC_Z;
    if((xh ^ vh) & (xh ^ rh) & 0x80) c.cc |= CC_V;
}
inline void setVfromNC(SoundCpu& c)
{
    const bool n = c.cc & CC_N, k = c.cc & CC_C;
    c.cc = static_cast<uint8_t>((c.cc & ~CC_V) | (n != k ? CC_V : 0));
}
inline uint8_t neg8(SoundCpu& c, uint8_t v) { return sub8(c, 0, v, 0); }
inline uint8_t com8(SoundCpu& c, uint8_t v)
{
    const uint8_t r = static_cast<uint8_t>(~v);
    logic8(c, r);
    c.cc |= CC_C;
    return r;
}
inline uint8_t lsr8(SoundCpu& c, uint8_t v)
{
    const uint8_t r = v >> 1;
    c.cc = static_cast<uint8_t>((c.cc & ~(CC_N | CC_Z | CC_C)) | (v & 1) | (r ? 0 : CC_Z));
    setVfromNC(c);
    return r;
}
inline uint8_t ror8(SoundCpu& c, uint8_t v)
{
    const uint8_t r = static_cast<uint8_t>((v >> 1) | ((c.cc & CC_C) << 7));
    nz8(c, r);
    c.cc = static_cast<uint8_t>((c.cc & ~CC_C) | (v & 1));
    setVfromNC(c);
    return r;
}
inline uint8_t asr8(SoundCpu& c, uint8_t v)
{
    const uint8_t r = static_cast<uint8_t>((v >> 1) | (v & 0x80));
    nz8(c, r);
    c.cc = static_cast<uint8_t>((c.cc & ~CC_C) | (v & 1));
    setVfromNC(c);
    return r;
}
inline uint8_t asl8(SoundCpu& c, uint8_t v)
{
    const uint8_t r = static_cast<uint8_t>(v << 1);
    nz8(c, r);
    c.cc = static_cast<uint8_t>((c.cc & ~CC_C) | (v >> 7));
    setVfromNC(c);
    return r;
}
inline uint8_t rol8(SoundCpu& c, uint8_t v)
{
    const uint8_t r = static_cast<uint8_t>((v << 1) | (c.cc & CC_C));
    nz8(c, r);
    c.cc = static_cast<uint8_t>((c.cc & ~CC_C) | (v >> 7));
    setVfromNC(c);
    return r;
}
inline uint8_t dec8(SoundCpu& c, uint8_t v)
{
    const uint8_t r = static_cast<uint8_t>(v - 1);
    nz8(c, r);
    c.cc = static_cast<uint8_t>((c.cc & ~CC_V) | (v == 0x80 ? CC_V : 0));
    return r;
}
inline uint8_t inc8(SoundCpu& c, uint8_t v)
{
    const uint8_t r = static_cast<uint8_t>(v + 1);
    nz8(c, r);
    c.cc = static_cast<uint8_t>((c.cc & ~CC_V) | (v == 0x7F ? CC_V : 0));
    return r;
}
inline void tst8(SoundCpu& c, uint8_t v)
{
    nz8(c, v);
    c.cc &= static_cast<uint8_t>(~(CC_V | CC_C));
}
inline uint8_t clr8(SoundCpu& c)
{
    c.cc = static_cast<uint8_t>((c.cc & ~(CC_N | CC_V | CC_C)) | CC_Z);
    return 0;
}
inline void daa(SoundCpu& c)
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
inline bool ge(const SoundCpu& c) { return !(c.cc & CC_N) == !(c.cc & CC_V); }
inline bool gt(const SoundCpu& c) { return !(c.cc & CC_Z) && ge(c); }

} // namespace df
