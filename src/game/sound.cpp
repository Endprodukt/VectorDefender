// The sound board around its translated program: see sound.hpp.
#include "game/sound.hpp"

#include <cstdio>

namespace df {

namespace {
#include "gen/sound_data.inc"
} // namespace

const uint8_t* const SoundBoard::rom = ROM_SOUND;

void SoundBoard::reset()
{
    const int64_t now = c.cyc;
    c = SoundCpu{};
    c.cyc = now;
    pia = Pia{};
    pia.inB = 0xFF;
    wai = false;
    c.pc = rd16(0xFFFE);
    dac_ = spanLevel_ = 0x80;
    sumFrom_ = now;
    changes_.clear();
}

void SoundBoard::command(uint8_t value)
{
    pia.inB = static_cast<uint8_t>(value | 0xC0);
    pia.setCB1(pia.inB != 0xFF ? 1 : 0);
    if(irq() && !(c.cc & CC_I)) limit = 0;
}

uint8_t SoundBoard::piaRead(int reg)
{
    switch(reg)
    {
        case 0:
            if(!(pia.cra & 0x04)) return pia.ddra;
            pia.cra &= 0x3F;
            return static_cast<uint8_t>((pia.ora & pia.ddra) | (0xFF & ~pia.ddra));
        case 1: return pia.cra;
        case 2:
            if(!(pia.crb & 0x04)) return pia.ddrb;
            pia.crb &= 0x3F;   // reading the sound number clears its interrupt
            return static_cast<uint8_t>((pia.orb & pia.ddrb) | (pia.inB & ~pia.ddrb));
        default: return pia.crb;
    }
}

void SoundBoard::piaWrite(int reg, uint8_t v)
{
    switch(reg)
    {
        case 0:
            if(pia.cra & 0x04) pia.ora = v;
            else pia.ddra = v;
            dacChange();
            break;
        case 1: pia.cra = static_cast<uint8_t>((pia.cra & 0xC0) | (v & 0x3F)); break;
        case 2:
            if(pia.crb & 0x04) pia.orb = v;
            else pia.ddrb = v;
            break;
        default: pia.crb = static_cast<uint8_t>((pia.crb & 0xC0) | (v & 0x3F)); break;
    }
    if(irq() && !(c.cc & CC_I)) limit = 0;
}

// The DAC takes its new level when the instruction writing it ends.
void SoundBoard::dacChange()
{
    const uint8_t level = pia.outA();
    if(level == dac_) return;
    changes_.push_back({c.cyc, level});
    dac_ = level;
}

void SoundBoard::bad(uint16_t pc)
{
    static bool told = false;
    if(!told) std::fprintf(stderr, "defender sound: no instruction at %04X\n", pc);
    told = true;
    c.cyc = limit > c.cyc ? limit : c.cyc + 1;
}

void SoundBoard::takeIrq()
{
    if(!wai)
    {
        push16(c.pc);
        push16(c.x);
        push8(c.a);
        push8(c.b);
        push8(c.cc);
    }
    wai = false;
    c.cc |= CC_I;
    c.pc = rd16(0xFFF8);
    c.cyc += 12;
}

void SoundBoard::runUntil(int64_t t)
{
    while(c.cyc < t)
    {
        if(irq() && !(c.cc & CC_I)) takeIrq();
        if(wai)
        {
            c.cyc = t;   // waiting for the main board's next sound
            break;
        }
        limit = t;
        const int64_t before = c.cyc;
        const uint16_t pc = c.pc;
        run_sound(*this);
        if(limit != 0 && c.cyc == before && c.pc == pc) break;   // reported by bad()
    }
}

void SoundBoard::takeSamples(float* out, int n)
{
    const int64_t from = sumFrom_, to = c.cyc;
    const double span = static_cast<double>(to - from);
    // the level of each sample: the DAC's levels, each for its cycles
    size_t k = 0;
    uint8_t level = spanLevel_;   // the level at `from`
    for(int i = 0; i < n; ++i)
    {
        const double s0 = static_cast<double>(from) + span * i / n, s1 = static_cast<double>(from) + span * (i + 1) / n;
        double acc = 0, t = s0;
        while(k < changes_.size() && static_cast<double>(changes_[k].first) < s1)
        {
            const double at = static_cast<double>(changes_[k].first);
            if(at > t)
            {
                acc += (level - 128.0) * (at - t);
                t = at;
            }
            level = changes_[k].second;
            ++k;
        }
        acc += (level - 128.0) * (s1 - t);
        const float in = static_cast<float>(acc / (s1 - s0 > 0 ? s1 - s0 : 1) / 128.0);
        // the board's output is AC coupled
        const float o = 0.9978f * (hpOut_ + in - hpIn_);
        hpIn_ = in;
        hpOut_ = o;
        out[i] = o * 0.45f;
    }
    spanLevel_ = level;
    changes_.clear();
    sumFrom_ = to;
}

} // namespace df
