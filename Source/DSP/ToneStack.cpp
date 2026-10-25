#include "ToneStack.h"

namespace abyssal
{

namespace
{
    // Pot positions as resistance fractions. Never exactly 0 or 1: a zero-ohm branch would make the
    // nodal matrix singular.
    double linearPot (float knob) { return juce::jlimit (1.0e-4, 1.0 - 1.0e-4, (double) knob / 10.0); }
    double audioPot (float knob)
    {
        const double x = juce::jlimit (0.0, 1.0, (double) knob / 10.0);
        return juce::jlimit (1.0e-4, 1.0, (std::exp (3.4 * x) - 1.0) / (std::exp (3.4) - 1.0));
    }

    /** Determinant of a small dense matrix (Gaussian elimination with partial pivoting). */
    template <int N>
    double determinant (std::array<std::array<double, N>, N> m)
    {
        double det = 1.0;
        for (int col = 0; col < N; ++col)
        {
            int pivot = col;
            for (int r = col + 1; r < N; ++r)
                if (std::abs (m[(size_t) r][(size_t) col]) > std::abs (m[(size_t) pivot][(size_t) col])) pivot = r;
            if (m[(size_t) pivot][(size_t) col] == 0.0)
                return 0.0;
            if (pivot != col) { std::swap (m[(size_t) pivot], m[(size_t) col]); det = -det; }
            det *= m[(size_t) col][(size_t) col];
            for (int r = col + 1; r < N; ++r)
            {
                const double f = m[(size_t) r][(size_t) col] / m[(size_t) col][(size_t) col];
                for (int k = col; k < N; ++k)
                    m[(size_t) r][(size_t) k] -= f * m[(size_t) col][(size_t) k];
            }
        }
        return det;
    }

    /** Solves the small Vandermonde system: coefficients of the polynomial through (x[i], y[i]). */
    template <int N>
    std::array<double, N> interpolate (const std::array<double, N>& x, const std::array<double, N>& y)
    {
        std::array<std::array<double, N + 1>, N> m {};
        for (int i = 0; i < N; ++i)
        {
            double p = 1.0;
            for (int k = 0; k < N; ++k) { m[(size_t) i][(size_t) k] = p; p *= x[(size_t) i]; }
            m[(size_t) i][(size_t) N] = y[(size_t) i];
        }
        for (int col = 0; col < N; ++col)
        {
            int pivot = col;
            for (int r = col + 1; r < N; ++r)
                if (std::abs (m[(size_t) r][(size_t) col]) > std::abs (m[(size_t) pivot][(size_t) col])) pivot = r;
            std::swap (m[(size_t) pivot], m[(size_t) col]);
            for (int r = 0; r < N; ++r)
            {
                if (r == col) continue;
                const double f = m[(size_t) r][(size_t) col] / m[(size_t) col][(size_t) col];
                for (int k = col; k <= N; ++k)
                    m[(size_t) r][(size_t) k] -= f * m[(size_t) col][(size_t) k];
            }
        }
        std::array<double, N> c {};
        for (int i = 0; i < N; ++i)
            c[(size_t) i] = m[(size_t) i][(size_t) N] / m[(size_t) i][(size_t) i];
        return c;
    }
}

ToneStack::Analog ToneStack::solve (const Components& c, float treble, float mid, float bass)
{
    const double t = linearPot (treble), md = linearPot (mid), l = audioPot (bass);

    // Everything is scaled by the slope resistor and a reference frequency so the numbers stay near 1.
    const double r0 = c.rSlope;
    const double w0 = 1.0 / (c.rSlope * c.c2);
    auto g = [r0] (double r) { return r0 / r; };          // scaled conductance
    auto y = [r0, w0] (double cap, double p) { return p * w0 * cap * r0; }; // scaled admittance of a cap at s = p * w0

    const double gTop = g ((1.0 - t) * c.rTreble), gBot = g (t * c.rTreble);
    const double gSlope = g (c.rSlope), gBass = g (l * c.rBass), gMid = g (md * c.rMid);

    // Unknown nodes: 0 = top of treble pot, 1 = OUT (wiper), 2 = x, 3 = b, 4 = m. Vin = 1 is a known source.
    // Returns det(Y) and det(Y with the OUT column replaced by the source currents), i.e. D(p) and N(p).
    auto evaluate = [&] (double p, double& num, double& den)
    {
        const double y1 = y (c.c1, p), y2 = y (c.c2, p), y3 = y (c.c3, p);
        std::array<std::array<double, 5>, 5> m {};
        m[0] = { y1 + gTop, -gTop, 0, 0, 0 };
        m[1] = { -gTop, gTop + gBot, 0, -gBot, 0 };
        m[2] = { 0, 0, gSlope + y2 + y3, -y2, -y3 };
        m[3] = { 0, -gBot, -y2, gBot + y2 + gBass, -gBass };
        m[4] = { 0, 0, -y3, -gBass, gBass + y3 + gMid };
        const std::array<double, 5> source { y1, 0, gSlope, 0, 0 };

        den = determinant<5> (m);
        for (int r = 0; r < 5; ++r) m[(size_t) r][1] = source[(size_t) r];
        num = determinant<5> (m);
    };

    // Both determinants are polynomials of degree <= 5 in p (in practice 3: three capacitors).
    constexpr int points = 6;
    const std::array<double, points> ps { 0.0, 1.0, -1.0, 2.0, -2.0, 3.0 };
    std::array<double, points> nums {}, dens {};
    for (int i = 0; i < points; ++i)
        evaluate (ps[(size_t) i], nums[(size_t) i], dens[(size_t) i]);
    const auto nc = interpolate<points> (ps, nums);
    const auto dc = interpolate<points> (ps, dens);

    // Back from p to s = p * w0.
    Analog a {};
    double scale = 1.0;
    for (int k = 0; k < 4; ++k)
    {
        a.num[k] = nc[(size_t) k] / scale;
        a.den[k] = dc[(size_t) k] / scale;
        scale *= w0;
    }
    return a;
}

void ToneStack::setup (const Components& c, double sampleRate, float treble, float mid, float bass)
{
    const auto an = solve (c, treble, mid, bass);

    // Bilinear transform of a cubic. The stack runs oversampled, so frequency warping is negligible.
    const double k = 2.0 * sampleRate, k2 = k * k, k3 = k2 * k;
    auto bilinear = [&] (const double* p, double* out)
    {
        out[0] = p[0] + p[1] * k + p[2] * k2 + p[3] * k3;
        out[1] = 3 * p[0] + p[1] * k - p[2] * k2 - 3 * p[3] * k3;
        out[2] = 3 * p[0] - p[1] * k - p[2] * k2 + 3 * p[3] * k3;
        out[3] = p[0] - p[1] * k + p[2] * k2 - p[3] * k3;
    };
    double nb[4], na[4];
    bilinear (an.num, nb);
    bilinear (an.den, na);
    for (int i = 0; i < 4; ++i)
    {
        b[i] = nb[i] / na[0];
        a[i] = na[i] / na[0];
    }
}

double ToneStack::magnitude (const Components& c, float treble, float mid, float bass, double freq)
{
    const auto an = solve (c, treble, mid, bass);
    const std::complex<double> s (0.0, juce::MathConstants<double>::twoPi * freq);
    std::complex<double> n = 0, d = 0, p = 1;
    for (int k = 0; k < 4; ++k)
    {
        n += an.num[k] * p;
        d += an.den[k] * p;
        p *= s;
    }
    return std::abs (n / d);
}

float ToneStack::noonMakeupDb (const Components& c)
{
    double sum = 0.0;
    int count = 0;
    for (double f = 80.0; f <= 5000.0; f *= 1.12, ++count)
        sum += 20.0 * std::log10 (std::max (1.0e-9, magnitude (c, 5.0f, 5.0f, 5.0f, f)));
    return (float) (-sum / count);
}

} // namespace abyssal
