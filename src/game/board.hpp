// The Defender board picture by picture: the translated program (Machine)
// and, beside it, the sound board (sound.hpp), with the line timing of the
// original. One call of frame() is one picture of the original's 60.1.
#pragma once

#include "game/cpu.hpp"
#include "game/sound.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace df {

// The cabinet's controls as the game reads them (MAME's IN0..IN2), active high.
enum : uint8_t
{
    IN0_FIRE = 0x01, IN0_THRUST = 0x02, IN0_SMART = 0x04, IN0_HYPER = 0x08,
    IN0_START2 = 0x10, IN0_START1 = 0x20, IN0_REVERSE = 0x40, IN0_DOWN = 0x80,
    IN1_UP = 0x01,
    IN2_AUTOUP = 0x01, IN2_ADVANCE = 0x02, IN2_COIN3 = 0x04, IN2_HSRESET = 0x08,
    IN2_COIN1 = 0x10, IN2_COIN2 = 0x20, IN2_TILT = 0x40
};

struct Inputs
{
    uint8_t in0{0}, in1{0}, in2{0};
};

constexpr int LINES = 260;
constexpr int LINE_CYCLES = 64;
constexpr int FRAME_CYCLES = LINES * LINE_CYCLES;
constexpr double FRAME_HZ = 1000000.0 / FRAME_CYCLES;   // 60.096

class Board
{
public:
    Machine m;
    SoundBoard snd;
    // The last picture's sound (frame()), mono.
    std::vector<float> audio;

    // Power on. cmosPath: the game's CMOS (settings, high scores), kept
    // between runs; empty = none.
    void powerOn(const std::string& cmosPath);
    // One picture; its sound as `samples` samples into `audio` (0: none).
    void frame(const Inputs& in, int samples = 0);
    void saveCmos();
    uint64_t frames() const { return frames_; }

    // A palette byte as colour (MAME's resistor weights).
    static void color(uint8_t v, uint8_t& r, uint8_t& g, uint8_t& b);
    // A pixel of the original's picture (the program still draws it):
    // 0..15, x 0..303, y 0..255.
    int pixel(int x, int y) const
    {
        const uint8_t v = m.ram[((x >> 1) << 8) | (y & 0xFF)];
        return (x & 1) ? (v & 0x0F) : (v >> 4);
    }

private:
    static void soundThunk(void* user, int64_t cycle, uint8_t value);
    std::string cmosPath_;
    int64_t sndStart_{0};      // the sound board's cycle at power on
    int64_t sinceOn_{0};       // pictures since power on
    uint64_t frames_{0};
};

} // namespace df
