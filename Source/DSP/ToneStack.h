#pragma once

#include "Common.h"

namespace abyssal
{

/** The passive Treble / Middle / Bass network found in Fender, Marshall and most high-gain amps,
    where every control interacts with the others.

    The circuit is solved exactly: nodal analysis gives the 3rd-order analog transfer function for
    the current pot positions, and the bilinear transform turns it into a digital filter.

        Vin --C1-- top --(1-t)Rt-- OUT --t Rt--+
         |                                     |
         +--Rs-- x --C2-----------------------  b --l Rb-- m --m Rm-- gnd
                 |                                       |
                 +--C3-----------------------------------+
*/
class ToneStack
{
public:
    struct Components
    {
        double c1, c2, c3;      // treble, bass, mid caps (F)
        double rTreble, rBass, rMid, rSlope;
    };

    /** Knobs 0..10. Treble and mid pots are linear, the bass pot is audio taper. */
    void setup (const Components& c, double sampleRate, float treble, float mid, float bass);
    void reset() noexcept { z[0] = z[1] = z[2] = 0.0; }

    inline float process (float x) noexcept
    {
        const double y = b[0] * x + z[0];
        z[0] = b[1] * x - a[1] * y + z[1];
        z[1] = b[2] * x - a[2] * y + z[2];
        z[2] = b[3] * x - a[3] * y;
        return (float) y;
    }

    /** Analog magnitude response, for tests and the makeup-gain calculation. */
    static double magnitude (const Components& c, float treble, float mid, float bass, double freq);

    /** Gain (dB) that brings the stack with every knob at 5 to roughly unity across the guitar range. */
    static float noonMakeupDb (const Components& c);

private:
    struct Analog { double num[4], den[4]; };
    static Analog solve (const Components& c, float treble, float mid, float bass);

    double b[4] { 1, 0, 0, 0 }, a[4] { 1, 0, 0, 0 };
    double z[3] {};
};

} // namespace abyssal
