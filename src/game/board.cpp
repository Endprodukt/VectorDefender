// The board picture by picture: see board.hpp.
#include "game/board.hpp"

#include <cmath>
#include <cstdio>

namespace df {

void Board::powerOn(const std::string& cmosPath)
{
    cmosPath_ = cmosPath;
    for(uint8_t& v : m.cmos) v = 0xF0;   // MAME: nvram all 0, read with the top nibble set
    if(!cmosPath_.empty())
        if(FILE* f = std::fopen(cmosPath_.c_str(), "rb"))
        {
            if(std::fread(m.cmos, 1, sizeof m.cmos, f) != sizeof m.cmos)
                for(uint8_t& v : m.cmos) v = 0xF0;
            std::fclose(f);
        }
    m.frameStart = m.c.cyc;
    m.reset();
    m.cmosDirty = false;
    m.onSound = &Board::soundThunk;
    m.soundUser = this;
    snd.reset();
    sndStart_ = snd.c.cyc;
    sinceOn_ = 0;
}

void Board::soundThunk(void* user, int64_t, uint8_t value)
{
    static_cast<Board*>(user)->snd.command(value);
}

void Board::saveCmos()
{
    if(cmosPath_.empty() || !m.cmosDirty) return;
    const std::string tmp = cmosPath_ + ".new";
    if(FILE* f = std::fopen(tmp.c_str(), "wb"))
    {
        const bool ok = std::fwrite(m.cmos, 1, sizeof m.cmos, f) == sizeof m.cmos;
        std::fclose(f);
        if(ok)
        {
            std::remove(cmosPath_.c_str());
            std::rename(tmp.c_str(), cmosPath_.c_str());
            m.cmosDirty = false;
        }
    }
}

void Board::frame(const Inputs& in, int samples)
{
    m.pia[1].inA = in.in0;
    m.pia[1].inB = in.in1;
    m.pia[0].inA = in.in2;
    const int64_t start = m.frameStart;
    for(int line = 0; line < LINES; ++line)
    {
        // MAME: VA11 into CB1 every 32 lines (not at 256), COUNT240 into CA1.
        if(line % 32 == 0 && line < 256) m.pia[0].setCB1(static_cast<uint8_t>((line >> 5) & 1));
        if(line == 0) m.pia[0].setCA1(0);
        if(line == 240) m.pia[0].setCA1(1);
        m.runUntil(start + static_cast<int64_t>(line + 1) * LINE_CYCLES);
        // the sound board alongside, line by line (its clock: 894.886 kHz)
        const double mainCycles = static_cast<double>(sinceOn_) * FRAME_CYCLES + (line + 1) * LINE_CYCLES;
        snd.runUntil(sndStart_ + static_cast<int64_t>(SOUND_CLOCK * mainCycles / 1000000.0));
    }
    m.frameStart = start + FRAME_CYCLES;
    ++sinceOn_;
    audio.resize(static_cast<size_t>(samples > 0 ? samples : 0));
    if(samples > 0) snd.takeSamples(audio.data(), samples);
    ++frames_;
}

void Board::color(uint8_t v, uint8_t& r, uint8_t& g, uint8_t& b)
{
    // williams_v.cpp: 3 bits red (1200, 560, 330 ohm), 3 green, 2 blue
    // (560, 330), as relative weights of the conductances.
    static float wr[3], wb[2];
    static bool ready = false;
    if(!ready)
    {
        const float g3[3] = {1.f / 1200.f, 1.f / 560.f, 1.f / 330.f};
        const float g2[2] = {1.f / 560.f, 1.f / 330.f};
        const float s3 = g3[0] + g3[1] + g3[2], s2 = g2[0] + g2[1];
        for(int i = 0; i < 3; ++i) wr[i] = 255.f * g3[i] / s3;
        for(int i = 0; i < 2; ++i) wb[i] = 255.f * g2[i] / s2;
        ready = true;
    }
    auto bits3 = [&](int x) { return wr[0] * (x & 1) + wr[1] * ((x >> 1) & 1) + wr[2] * ((x >> 2) & 1); };
    r = static_cast<uint8_t>(std::lround(bits3(v & 7)));
    g = static_cast<uint8_t>(std::lround(bits3((v >> 3) & 7)));
    b = static_cast<uint8_t>(std::lround(wb[0] * ((v >> 6) & 1) + wb[1] * ((v >> 7) & 1)));
}

} // namespace df
