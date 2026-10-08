#include "FactorySamples.h"
#include <mutex>

namespace rs
{
namespace
{
    constexpr double sr = factorySampleRate;
    constexpr double twoPi = juce::MathConstants<double>::twoPi;
    constexpr int beatsPerSample = 8;

    double hz (double midiNote) { return 440.0 * std::pow (2.0, (midiNote - 69.0) / 12.0); }
    int samplesFor (double seconds) { return juce::jmax (1, (int) (seconds * sr)); }

    //==============================================================================
    // Building blocks

    struct Noise
    {
        explicit Noise (juce::uint32 seed) : state (seed * 2654435761u + 1u) {}
        float next()
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return (float) state / 2147483648.0f - 1.0f;
        }
        juce::uint32 state;
    };

    double polyBlep (double t, double dt)
    {
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0;
        }
        if (t > 1.0 - dt)
        {
            t = (t - 1.0) / dt;
            return t * t + t + t + 1.0;
        }
        return 0.0;
    }

    struct Osc
    {
        double phase = 0.0;
        float saw (double freq)
        {
            const double dt = freq / sr;
            const double v = 2.0 * phase - 1.0 - polyBlep (phase, dt);
            advance (dt);
            return (float) v;
        }
        float pulse (double freq, double width = 0.5)
        {
            const double dt = freq / sr;
            double v = phase < width ? 1.0 : -1.0;
            v += polyBlep (phase, dt);
            v -= polyBlep (std::fmod (phase + 1.0 - width, 1.0), dt);
            advance (dt);
            return (float) v;
        }
        float sine (double freq)
        {
            const double v = std::sin (twoPi * phase);
            advance (freq / sr);
            return (float) v;
        }
        void advance (double dt)
        {
            phase += dt;
            phase -= std::floor (phase);
        }
    };

    /** Topology-preserving state variable filter (Simper). */
    struct Svf
    {
        float ic1 = 0.0f, ic2 = 0.0f, lp = 0.0f, bp = 0.0f, hp = 0.0f;
        void process (float x, double cutoff, double q)
        {
            const double g = std::tan (juce::MathConstants<double>::pi * juce::jlimit (10.0, sr * 0.45, cutoff) / sr);
            const double k = 1.0 / q;
            const double a1 = 1.0 / (1.0 + g * (g + k));
            const double a2 = g * a1, a3 = g * a2;
            const double v3 = x - ic2;
            const double v1 = a1 * ic1 + a2 * v3;
            const double v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = (float) (2.0 * v1 - ic1);
            ic2 = (float) (2.0 * v2 - ic2);
            lp = (float) v2;
            bp = (float) v1;
            hp = (float) (x - k * v1 - v2);
        }
    };

    float attackDecay (double t, double attack, double decay)
    {
        return (float) (t < attack ? t / attack : std::exp (-(t - attack) / decay));
    }

    /** Stereo canvas the length of the sample. Sounds that run past the end wrap
        around to the start, so loops play seamlessly. */
    struct Canvas
    {
        Canvas (double bpmIn) : bpm (bpmIn), l ((size_t) samplesFor (beatsPerSample * 60.0 / bpmIn)), r (l.size()) {}

        double beat() const { return 60.0 / bpm; }
        double step() const { return beat() / 4.0; }

        void add (double atSeconds, const std::vector<float>& sig, float gain = 1.0f, float pan = 0.0f)
        {
            const float gl = gain * std::sqrt (0.5f * (1.0f - pan));
            const float gr = gain * std::sqrt (0.5f * (1.0f + pan));
            const size_t size = l.size();
            size_t idx = (size_t) juce::jmax (0, samplesFor (atSeconds) - 1) % size;
            for (float v : sig)
            {
                l[idx] += v * gl;
                r[idx] += v * gr;
                if (++idx == size)
                    idx = 0;
            }
        }

        void addStereo (double atSeconds, const std::vector<float>& left, const std::vector<float>& right, float gain = 1.0f)
        {
            add (atSeconds, left, gain * std::sqrt (2.0f), -1.0f);
            add (atSeconds, right, gain * std::sqrt (2.0f), 1.0f);
        }

        /** Calls fn for each '*'-free non '.' character in a 16th-note pattern. */
        template <typename Fn>
        void pattern (const char* steps, Fn&& fn, double swing = 0.0)
        {
            for (int i = 0; steps[i] != 0; ++i)
            {
                if (steps[i] == '.' || steps[i] == ' ')
                    continue;
                const double t = i * step() + ((i % 2) == 1 ? swing * step() : 0.0);
                fn (t, steps[i]);
            }
        }

        double bpm;
        std::vector<float> l, r;
    };

    std::vector<float> render (double seconds, const std::function<float (double)>& fn)
    {
        std::vector<float> out ((size_t) samplesFor (seconds));
        for (size_t i = 0; i < out.size(); ++i)
            out[i] = fn ((double) i / sr);
        return out;
    }

    //==============================================================================
    // Drums

    std::vector<float> kick (double tune = 1.0, double decay = 0.28, juce::uint32 seed = 1)
    {
        Osc osc;
        Noise noise (seed);
        return render (0.7, [&] (double t)
        {
            const double f = (45.0 + 115.0 * std::exp (-t / 0.045)) * tune;
            const float body = osc.sine (f) * (float) std::exp (-t / decay);
            const float click = noise.next() * (float) std::exp (-t / 0.003) * 0.35f;
            return std::tanh (1.6f * (body + click));
        });
    }

    std::vector<float> snare (double tone = 185.0, double decay = 0.12, juce::uint32 seed = 2)
    {
        Osc osc;
        Noise noise (seed);
        Svf hp;
        return render (0.45, [&] (double t)
        {
            hp.process (noise.next(), 1500.0, 0.7);
            const float body = osc.sine (tone * (1.0 + 0.3 * std::exp (-t / 0.01))) * (float) std::exp (-t / 0.07) * 0.6f;
            return body + hp.hp * (float) std::exp (-t / decay) * 0.8f;
        });
    }

    std::vector<float> clap (juce::uint32 seed = 3)
    {
        Noise noise (seed);
        Svf bp;
        return render (0.5, [&] (double t)
        {
            bp.process (noise.next(), 1300.0, 1.2);
            double env = 0.0;
            for (double start : { 0.0, 0.011, 0.023 })
                if (t >= start)
                    env += std::exp (-(t - start) / 0.006);
            if (t >= 0.03)
                env += 0.8 * std::exp (-(t - 0.03) / 0.13);
            return bp.bp * (float) env * 1.4f;
        });
    }

    std::vector<float> hat (bool open, juce::uint32 seed = 4)
    {
        Noise noise (seed);
        Svf hp;
        Osc m1, m2;
        return render (open ? 0.5 : 0.1, [&] (double t)
        {
            // A little metallic ring under the noise.
            const float metal = 0.3f * (m1.pulse (540.0) * m2.pulse (800.0));
            hp.process (noise.next() + metal, 7500.0, 0.8);
            return hp.hp * (float) std::exp (-t / (open ? 0.14 : 0.025)) * 0.55f;
        });
    }

    std::vector<float> tom (double pitch, juce::uint32 seed = 5)
    {
        Osc osc;
        Noise noise (seed);
        return render (0.6, [&] (double t)
        {
            const double f = pitch * (1.0 + 0.6 * std::exp (-t / 0.04));
            return osc.sine (f) * (float) std::exp (-t / 0.22) + noise.next() * (float) std::exp (-t / 0.005) * 0.2f;
        });
    }

    std::vector<float> rim (juce::uint32 seed = 6)
    {
        Osc a, b;
        Noise noise (seed);
        return render (0.12, [&] (double t)
        {
            return (a.sine (1700.0) * 0.6f + b.sine (520.0) * 0.4f) * (float) std::exp (-t / 0.018)
                 + noise.next() * (float) std::exp (-t / 0.003) * 0.4f;
        });
    }

    std::vector<float> shaker (juce::uint32 seed = 7)
    {
        Noise noise (seed);
        Svf bp;
        return render (0.12, [&] (double t)
        {
            bp.process (noise.next(), 6000.0, 1.5);
            return bp.bp * attackDecay (t, 0.012, 0.035) * 0.8f;
        });
    }

    std::vector<float> bass808 (double note, double length, double glideTo = 0.0, double drive = 2.0)
    {
        Osc osc;
        const double f0 = hz (note);
        const double f1 = glideTo > 0.0 ? hz (glideTo) : f0;
        return render (length, [&] (double t)
        {
            const double glide = glideTo > 0.0 ? juce::jlimit (0.0, 1.0, (t - length * 0.45) / 0.08) : 0.0;
            const double f = (f0 + (f1 - f0) * glide) * (1.0 + 0.8 * std::exp (-t / 0.015));
            const float env = attackDecay (t, 0.002, length * 1.5) * (float) juce::jlimit (0.0, 1.0, (length - t) / 0.02);
            return std::tanh ((float) drive * osc.sine (f) * env) * 0.8f;
        });
    }

    //==============================================================================
    // Tonal voices

    std::vector<float> rhodes (double note, double length, double bright = 1.0)
    {
        Osc carrier, modulator, bell;
        const double f = hz (note);
        return render (length + 0.6, [&] (double t)
        {
            const double index = (1.6 * std::exp (-t / 0.22) + 0.25) * bright;
            const double mod = modulator.sine (f) * index;
            const float tone = (float) std::sin (twoPi * carrier.phase + mod);
            carrier.advance (f / sr);
            const float tine = bell.sine (f * 7.0) * (float) std::exp (-t / 0.05) * 0.08f;
            const float release = t > length ? (float) std::exp (-(t - length) / 0.12) : 1.0f;
            return (tone + tine) * attackDecay (t, 0.003, 1.8) * release * 0.35f;
        });
    }

    std::vector<float> fmBell (double note, double length, double ratio = 3.5)
    {
        Osc carrier, modulator;
        const double f = hz (note);
        return render (length, [&] (double t)
        {
            const double index = 3.0 * std::exp (-t / 0.35);
            const double mod = modulator.sine (f * ratio) * index;
            const float tone = (float) std::sin (twoPi * carrier.phase + mod);
            carrier.advance (f / sr);
            return tone * attackDecay (t, 0.002, 1.1) * 0.4f;
        });
    }

    std::vector<float> supersaw (double note, double length, double cutoff, double attack, double release, double detuneCents, juce::uint32 seed)
    {
        std::array<Osc, 5> oscs;
        Noise rng (seed);
        for (auto& o : oscs)
            o.phase = (rng.next() + 1.0f) * 0.5f;
        Svf filter;
        const double f = hz (note);
        return render (length + release * 3.0, [&] (double t)
        {
            float sum = 0.0f;
            for (size_t i = 0; i < oscs.size(); ++i)
            {
                const double cents = detuneCents * ((double) i - 2.0) / 2.0;
                sum += oscs[i].saw (f * std::pow (2.0, cents / 1200.0));
            }
            filter.process (sum * 0.2f, cutoff * (1.0 + 0.25 * std::sin (twoPi * 0.3 * t)), 0.8);
            const float env = (float) juce::jmin (1.0, t / attack) * (t > length ? (float) std::exp (-(t - length) / release) : 1.0f);
            return filter.lp * env;
        });
    }

    std::vector<float> pluck (double note, double length, double brightness, double width)
    {
        Osc osc;
        Svf filter;
        const double f = hz (note);
        return render (length, [&] (double t)
        {
            const double cutoff = 250.0 + brightness * std::exp (-t / 0.09);
            filter.process (osc.pulse (f, width) * 0.5f, cutoff, 1.6);
            return filter.lp * attackDecay (t, 0.002, 0.35) * 0.7f;
        });
    }

    std::vector<float> karplus (double note, double length, juce::uint32 seed)
    {
        const int period = juce::jmax (2, (int) std::round (sr / hz (note)));
        std::vector<float> line ((size_t) period);
        Noise noise (seed);
        float prev = 0.0f;
        for (auto& v : line)
        {
            const float n = noise.next();
            v = 0.5f * (n + prev); // soften the excitation
            prev = n;
        }
        size_t idx = 0;
        return render (length, [&] (double t)
        {
            const float out = line[idx];
            const float next = line[(idx + 1) % line.size()];
            line[idx] = 0.4985f * (out + next);
            idx = (idx + 1) % line.size();
            return out * 0.6f * (float) juce::jlimit (0.0, 1.0, (length - t) / 0.05);
        });
    }

    std::vector<float> organ (double note, double length)
    {
        static constexpr double harmonics[] { 0.5, 1.0, 2.0, 3.0, 4.0, 6.0, 8.0 };
        static constexpr float weights[] { 0.5f, 1.0f, 0.8f, 0.5f, 0.4f, 0.25f, 0.2f };
        std::array<Osc, 7> oscs;
        Noise noise (11);
        const double f = hz (note);
        return render (length + 0.05, [&] (double t)
        {
            float sum = 0.0f;
            for (size_t i = 0; i < oscs.size(); ++i)
                sum += oscs[i].sine (f * harmonics[i]) * weights[i];
            const float click = noise.next() * (float) std::exp (-t / 0.002) * 0.3f;
            const float rotary = 1.0f + 0.15f * (float) std::sin (twoPi * 6.2 * t);
            const float env = (float) juce::jmin (1.0, t / 0.004) * (float) juce::jlimit (0.0, 1.0, (length + 0.05 - t) / 0.05);
            return (sum * 0.12f * rotary + click) * env;
        });
    }

    struct Vowel { double f1, f2, f3; };
    constexpr Vowel vowels[] { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 }, { 570, 840, 2410 }, { 300, 870, 2240 } };

    std::vector<float> voice (double note, double length, int vowel, int nextVowel, juce::uint32 seed)
    {
        Osc glottal, vibrato;
        Noise breath (seed);
        Svf a, b, c;
        const double f = hz (note);
        const auto& v0 = vowels[vowel];
        const auto& v1 = vowels[nextVowel];
        return render (length, [&] (double t)
        {
            const double m = juce::jlimit (0.0, 1.0, t / length);
            const double vib = 1.0 + 0.006 * vibrato.sine (5.2) * juce::jmin (1.0, t / 0.3);
            const float src = glottal.saw (f * vib) * 0.6f + breath.next() * 0.08f;
            a.process (src, v0.f1 + (v1.f1 - v0.f1) * m, 9.0);
            b.process (src, v0.f2 + (v1.f2 - v0.f2) * m, 11.0);
            c.process (src, v0.f3 + (v1.f3 - v0.f3) * m, 12.0);
            const float env = attackDecay (t, 0.04, length * 0.9) * (float) juce::jlimit (0.0, 1.0, (length - t) / 0.06);
            return (a.bp + b.bp * 0.6f + c.bp * 0.3f) * env * 0.9f;
        });
    }

    void addChord (Canvas& canvas, double at, std::initializer_list<double> notes, const std::function<std::vector<float> (double)>& make, float gain)
    {
        float pan = -0.3f;
        for (double n : notes)
        {
            canvas.add (at, make (n), gain, pan);
            pan += 0.6f / (float) juce::jmax<size_t> (1, notes.size() - 1);
        }
    }

    void addVinyl (Canvas& canvas, float amount, juce::uint32 seed)
    {
        Noise noise (seed);
        Svf hiss;
        for (size_t i = 0; i < canvas.l.size(); ++i)
        {
            hiss.process (noise.next(), 4000.0, 0.5);
            float v = hiss.lp * 0.02f * amount;
            if (noise.next() > 0.9993f)
                v += noise.next() * 0.25f * amount; // crackle
            canvas.l[i] += v;
            canvas.r[i] += v;
        }
    }

    //==============================================================================
    // The sounds

    void dustyBoomBap (Canvas& c)
    {
        const double swing = 0.18;
        c.pattern ("x.........x.x...x.......x.x.....", [&] (double t, char) { c.add (t, kick (1.0, 0.3), 0.95f); }, swing);
        c.pattern ("....x.......x.......x.......x...", [&] (double t, char) { c.add (t, snare (190.0, 0.14), 0.75f); }, swing);
        c.pattern ("x.x.x.x.x.x.x.xxx.x.x.x.x.x.x.x.", [&] (double t, char) { c.add (t, hat (false, (juce::uint32) (t * 1000)), 0.4f, 0.2f); }, swing);
        c.pattern ("...............x.............o..", [&] (double t, char) { c.add (t, hat (true), 0.3f, 0.2f); }, swing);
        addVinyl (c, 1.0f, 99);
    }

    void nightTrap (Canvas& c)
    {
        c.pattern ("x......x..x.....x.....x...x.....", [&] (double t, char) { c.add (t, kick (1.05, 0.2), 0.9f); });
        c.pattern ("........x...............x.......", [&] (double t, char) { c.add (t, clap(), 0.7f); c.add (t, snare (220.0, 0.1), 0.4f); });
        c.pattern ("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", [&] (double t, char) { c.add (t, hat (false, (juce::uint32) (t * 997)), 0.32f, 0.25f); });
        // Hi-hat roll in 32nd notes before the second bar.
        for (int i = 0; i < 6; ++i)
            c.add (c.step() * (12.0 + i * 0.5), hat (false, 50u + (juce::uint32) i), 0.22f + 0.03f * (float) i, 0.25f);
        const double notes[] { 33, 33, 36, 31 };
        const double starts[] { 0, 7, 10, 16 };
        const double lens[] { 1.6, 0.7, 1.4, 3.6 };
        for (int i = 0; i < 4; ++i)
            c.add (starts[i] * c.step(), bass808 (notes[i], lens[i] * c.beat(), i == 3 ? 28.0 : 0.0), 0.8f);
    }

    void warehouseHouse (Canvas& c)
    {
        c.pattern ("x...x...x...x...x...x...x...x...", [&] (double t, char) { c.add (t, kick (0.95, 0.24), 0.95f); });
        c.pattern ("....x.......x.......x.......x...", [&] (double t, char) { c.add (t, clap(), 0.6f, -0.1f); });
        c.pattern ("..x...x...x...x...x...x...x...x.", [&] (double t, char) { c.add (t, hat (true, (juce::uint32) (t * 333)), 0.35f, 0.15f); });
        c.pattern ("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", [&] (double t, char) { c.add (t, shaker ((juce::uint32) (t * 777)), 0.25f, -0.35f); });
        c.pattern ("...x.......x..x....x.......x....", [&] (double t, char) { c.add (t, rim(), 0.3f, 0.4f); });
    }

    void percKit (Canvas& c)
    {
        c.add (0 * c.beat(), kick (1.0, 0.35), 1.0f);
        c.add (1 * c.beat(), snare (200.0, 0.15), 0.8f);
        c.add (2 * c.beat(), clap(), 0.75f);
        c.add (3 * c.beat(), hat (false), 0.7f);
        c.add (4 * c.beat(), hat (true), 0.6f);
        c.add (5 * c.beat(), tom (95.0), 0.8f);
        c.add (6 * c.beat(), rim(), 0.7f);
        c.add (7 * c.beat(), bass808 (36, c.beat() * 0.95), 0.9f);
    }

    void lofiKeys (Canvas& c)
    {
        const auto make = [&] (double len) { return [len] (double n) { return rhodes (n, len, 0.8); }; };
        addChord (c, 0 * c.beat(), { 50, 53, 57, 60, 64 }, make (c.beat() * 1.9), 0.5f); // Dm9
        addChord (c, 2 * c.beat(), { 43, 53, 59, 64, 69 }, make (c.beat() * 1.9), 0.5f); // G13
        addChord (c, 4 * c.beat(), { 48, 52, 55, 59, 62 }, make (c.beat() * 1.9), 0.5f); // Cmaj9
        addChord (c, 6 * c.beat(), { 45, 55, 60, 64, 71 }, make (c.beat() * 1.9), 0.5f); // Am9
        addVinyl (c, 0.8f, 7);
    }

    void glassPad (Canvas& c)
    {
        const double len = c.beat() * 3.7;
        const auto pad = [&] (juce::uint32 seed) { return [len, seed] (double n) { return supersaw (n, len, 2200.0, 0.5, 0.4, 18.0, seed + (juce::uint32) n); }; };
        addChord (c, 0, { 53, 57, 60, 64, 72 }, pad (1), 0.5f); // Fmaj7
        addChord (c, 4 * c.beat(), { 52, 55, 59, 62, 71 }, pad (2), 0.5f); // Em7
        for (int i = 0; i < 8; ++i)
            c.add (i * c.beat() + c.step() * 2, fmBell (84.0 + (i % 2 == 0 ? 0.0 : 7.0), 1.2, 2.0), 0.08f, (i % 2) ? 0.6f : -0.6f);
    }

    void subBassline (Canvas& c)
    {
        const double notes[] { 28, 0, 28, 31, 0, 33, 31, 28, 28, 0, 40, 38, 0, 35, 33, 31 };
        for (int i = 0; i < 16; ++i)
        {
            if (notes[i] <= 0)
                continue;
            Osc o;
            const double f = hz (notes[i]);
            const double len = c.step() * 1.8;
            c.add (i * c.step() * 2, render (len, [&] (double t)
            {
                const float s = o.sine (f);
                return (s + 0.25f * s * s * s) * attackDecay (t, 0.004, 0.25) * (float) juce::jlimit (0.0, 1.0, (len - t) / 0.01) * 0.8f;
            }), 1.0f);
        }
    }

    void reesePressure (Canvas& c)
    {
        const double notes[] { 28, 31, 26 };
        const double starts[] { 0, 4, 6 };
        const double lens[] { 4, 2, 2 };
        for (int n = 0; n < 3; ++n)
        {
            Osc a, b, sub;
            Svf filter;
            const double f = hz (notes[n]);
            const double len = lens[n] * c.beat();
            c.add (starts[n] * c.beat(), render (len, [&] (double t)
            {
                const float saw = a.saw (f * 1.009) + b.saw (f * 0.991);
                filter.process (saw * 0.4f, 300.0 + 500.0 * (0.5 + 0.5 * std::sin (twoPi * 0.9 * t)), 1.2);
                const float env = (float) juce::jmin (1.0, t / 0.01) * (float) juce::jlimit (0.0, 1.0, (len - t) / 0.03);
                return (std::tanh (filter.lp * 2.0f) * 0.5f + sub.sine (f) * 0.5f) * env;
            }), 0.9f);
        }
        c.pattern ("x.........x.....x.........x.....", [&] (double t, char) { c.add (t, kick (1.1, 0.18), 0.6f); });
        c.pattern ("....x.......x.......x.......x...", [&] (double t, char) { c.add (t, snare (210.0, 0.1), 0.45f); });
    }

    void neonPluckArp (Canvas& c)
    {
        const std::array<std::array<double, 4>, 4> chords { { { 57, 60, 64, 69 }, { 53, 57, 60, 65 }, { 48, 55, 60, 64 }, { 55, 59, 62, 67 } } };
        for (int i = 0; i < 32; ++i)
        {
            const auto& chord = chords[(size_t) (i / 8)];
            static constexpr int order[] { 0, 1, 2, 3, 2, 1, 3, 2 };
            const double note = chord[(size_t) order[i % 8]] + ((i % 8) == 6 ? 12.0 : 0.0);
            c.add (i * c.step(), pluck (note, 0.5, 5200.0, 0.35), 0.45f, (i % 2) ? 0.5f : -0.5f);
        }
        c.pattern ("x...x...x...x...x...x...x...x...", [&] (double t, char) { c.add (t, pluck (33, 0.3, 900.0, 0.5), 0.6f); });
    }

    void ghostVox (Canvas& c)
    {
        const double melody[] { 72, 70, 67, 70, 72, 75, 74, 70 };
        const int syllables[] { 0, 3, 1, 0, 4, 0, 1, 3 };
        for (int i = 0; i < 8; ++i)
            c.add (i * c.beat(), voice (melody[i], c.beat() * 0.92, syllables[i], syllables[(i + 1) % 8], (juce::uint32) i + 3), 0.8f, i % 2 ? 0.15f : -0.15f);
    }

    void musicBoxBells (Canvas& c)
    {
        const double melody[] { 84, 86, 88, 91, 93, 91, 88, 86, 84, 88, 91, 96, 93, 91, 88, 91 };
        for (int i = 0; i < 16; ++i)
            c.add (i * c.step() * 2, fmBell (melody[i], 1.5), 0.55f, (float) std::sin (i * 0.9) * 0.5f);
    }

    void nylonPluck (Canvas& c)
    {
        const double chordA[] { 50, 57, 62, 66, 69, 66, 62, 57 };
        const double chordB[] { 47, 54, 59, 62, 66, 62, 59, 54 };
        for (int i = 0; i < 16; ++i)
        {
            const double note = i < 8 ? chordA[i] : chordB[i - 8];
            c.add (i * c.step() * 2, karplus (note, 1.4, (juce::uint32) (i + 1)), 0.6f, (float) ((i % 4) - 1.5) * 0.15f);
        }
    }

    void organStabs (Canvas& c)
    {
        const auto stab = [&] (double len) { return [len] (double n) { return organ (n, len); }; };
        c.pattern ("..x...x...x...x...x...x...x...x.", [&] (double t, char)
        {
            const bool second = t >= 4.0 * c.beat();
            if (second)
                addChord (c, t, { 57, 60, 64, 67 }, stab (0.16), 0.45f);
            else
                addChord (c, t, { 55, 59, 62, 65 }, stab (0.16), 0.45f);
        });
        c.pattern ("x...x...x...x...x...x...x...x...", [&] (double t, char) { c.add (t, kick (0.95, 0.22), 0.6f); });
    }

    void tapeStrings (Canvas& c)
    {
        const double len = c.beat() * 3.8;
        auto make = [&] (juce::uint32 seed) { return [len, seed] (double n) { return supersaw (n, len, 3200.0, 0.35, 0.5, 9.0, seed + (juce::uint32) n * 3); }; };
        addChord (c, 0, { 48, 55, 58, 62, 63 }, make (5), 0.55f);          // Cm9
        addChord (c, 4 * c.beat(), { 44, 51, 55, 60, 63 }, make (9), 0.55f); // Abmaj7
        // Tape wow: slowly modulated fractional delay.
        std::vector<float> l = c.l, r = c.r;
        for (size_t i = 0; i < c.l.size(); ++i)
        {
            const double d = 40.0 + 25.0 * std::sin (twoPi * 0.45 * (double) i / sr);
            const double pos = (double) i - d;
            const auto i0 = (size_t) ((long) std::floor (pos) + (long) c.l.size()) % c.l.size();
            const auto i1 = (i0 + 1) % c.l.size();
            const float frac = (float) (pos - std::floor (pos));
            c.l[i] = l[i0] + frac * (l[i1] - l[i0]);
            c.r[i] = r[i0] + frac * (r[i1] - r[i0]);
        }
        addVinyl (c, 0.5f, 21);
    }

    void glide808 (Canvas& c)
    {
        c.add (0, bass808 (31, c.beat() * 1.4, 0.0, 3.0), 0.9f);
        c.add (c.step() * 6, bass808 (31, c.beat() * 0.9, 34.0, 3.0), 0.9f);
        c.add (c.step() * 12, bass808 (29, c.beat() * 1.0, 0.0, 3.0), 0.9f);
        c.add (c.step() * 16, bass808 (31, c.beat() * 1.4, 0.0, 3.0), 0.9f);
        c.add (c.step() * 22, bass808 (36, c.beat() * 0.9, 31.0, 3.0), 0.9f);
        c.add (c.step() * 28, bass808 (26, c.beat() * 1.0, 0.0, 3.0), 0.9f);
        c.pattern ("x.....x.....x...x.....x.....x...", [&] (double t, char) { c.add (t, kick (1.05, 0.12), 0.5f); });
        c.pattern ("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", [&] (double t, char) { c.add (t, hat (false, (juce::uint32) (t * 555)), 0.22f, 0.3f); });
    }

    void voidTexture (Canvas& c)
    {
        Noise noise (77);
        Svf a, b;
        Osc d1, d2, d3;
        const double total = (double) c.l.size() / sr;
        for (size_t i = 0; i < c.l.size(); ++i)
        {
            const double t = (double) i / sr;
            const double sweep = 300.0 * std::pow (16.0, 0.5 + 0.5 * std::sin (twoPi * t / total));
            const float n = noise.next();
            a.process (n, sweep, 6.0);
            b.process (n, sweep * 1.5, 6.0);
            const float drone = d1.sine (55.0) * 0.3f + d2.sine (82.5) * 0.2f + d3.sine (110.0 * 1.003) * 0.1f;
            c.l[i] += a.bp * 0.5f + drone * 0.6f;
            c.r[i] += b.bp * 0.5f + drone * 0.6f;
        }
        for (int i = 0; i < 8; ++i)
            c.add (i * c.beat() + c.step(), fmBell (79.0 + (i * 5) % 12, 2.0, 1.41), 0.15f, i % 2 ? 0.7f : -0.7f);
    }

    void chiptuneLead (Canvas& c)
    {
        const double melody[] { 76, 79, 83, 79, 76, 72, 74, 76, 79, 81, 79, 76, 74, 71, 72, 74,
                                76, 79, 83, 86, 84, 83, 79, 76, 79, 81, 83, 81, 79, 76, 74, 72 };
        for (int i = 0; i < 32; ++i)
        {
            Osc o, vib;
            const double f = hz (melody[i]);
            const double len = c.step() * 0.95;
            c.add (i * c.step(), render (len, [&] (double t)
            {
                return o.pulse (f * (1.0 + 0.004 * vib.sine (7.0)), 0.25) * 0.3f * (float) juce::jlimit (0.0, 1.0, (len - t) / 0.004);
            }), 0.7f, 0.1f);
        }
        c.pattern ("x...x...x...x...x...x...x...x...", [&] (double t, char)
        {
            Osc o;
            c.add (t, render (c.step() * 1.5, [&] (double tt) { return o.pulse (hz (40.0), 0.5) * 0.25f * (float) std::exp (-tt / 0.1); }), 0.8f, -0.1f);
        });
        c.pattern ("....x.......x.......x.......x...", [&] (double t, char) { c.add (t, snare (300.0, 0.06), 0.4f); });
    }

    void soulChops (Canvas& c)
    {
        const auto keys = [&] (double len) { return [len] (double n) { return rhodes (n, len, 1.3); }; };
        addChord (c, 0, { 51, 58, 62, 65, 67 }, keys (c.beat() * 1.9), 0.45f);           // Ebmaj9
        addChord (c, 2 * c.beat(), { 50, 57, 60, 65 }, keys (c.beat() * 1.9), 0.45f);    // Dm7
        addChord (c, 4 * c.beat(), { 43, 58, 62, 65, 69 }, keys (c.beat() * 1.9), 0.45f); // Gm9
        addChord (c, 6 * c.beat(), { 48, 58, 62, 64, 67 }, keys (c.beat() * 1.9), 0.45f); // C9
        for (size_t i = 0; i < c.l.size(); ++i)
        {
            c.l[i] = std::tanh (c.l[i] * 1.8f) * 0.7f;
            c.r[i] = std::tanh (c.r[i] * 1.8f) * 0.7f;
        }
        addVinyl (c, 1.4f, 33);
    }

    void afroPerc (Canvas& c)
    {
        c.pattern ("x.....x.....x...x.....x.....x...", [&] (double t, char) { c.add (t, kick (0.9, 0.25), 0.8f); });
        c.pattern ("..x.x...x.x.x...x.x...x.x.x...x.", [&] (double t, char) { c.add (t, tom (190.0 + std::fmod (t * 37.0, 3.0) * 40.0, (juce::uint32) (t * 10)), 0.45f, 0.3f); });
        c.pattern ("x..x..x...x..x..x..x..x...x..x..", [&] (double t, char) { c.add (t, rim(), 0.4f, -0.4f); });
        c.pattern ("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", [&] (double t, char) { c.add (t, shaker ((juce::uint32) (t * 999)), 0.3f, 0.5f); });
    }

    void deepChords (Canvas& c)
    {
        const auto stab = [&] (double len) { return [len] (double n) { return supersaw (n, len, 1400.0, 0.003, 0.15, 12.0, (juce::uint32) n); }; };
        c.pattern ("..x....x..x...x...x....x..x.....", [&] (double t, char)
        {
            addChord (c, t, { 57, 60, 64, 67, 71 }, stab (c.step() * 1.5), 0.55f);
        });
        c.pattern ("x...x...x...x...x...x...x...x...", [&] (double t, char) { c.add (t, kick (0.92, 0.24), 0.8f); });
        c.pattern ("..x...x...x...x...x...x...x...x.", [&] (double t, char) { c.add (t, hat (false, (juce::uint32) (t * 77)), 0.3f, 0.2f); });
    }

    //==============================================================================
    struct Entry
    {
        FactorySampleInfo info;
        void (*make) (Canvas&);
    };

    const std::array<Entry, 20>& entries()
    {
        static const std::array<Entry, 20> list { {
            { { "Dusty Boom Bap", "Drums", 90.0 }, dustyBoomBap },
            { { "Night Trap", "Drums", 140.0 }, nightTrap },
            { { "Warehouse House", "Drums", 124.0 }, warehouseHouse },
            { { "Perc Kit", "Drums", 120.0 }, percKit },
            { { "Afro Perc", "Drums", 105.0 }, afroPerc },
            { { "Lofi Keys", "Keys", 80.0 }, lofiKeys },
            { { "Soul Chops", "Keys", 92.0 }, soulChops },
            { { "Organ Stabs", "Keys", 118.0 }, organStabs },
            { { "Deep Chords", "Keys", 122.0 }, deepChords },
            { { "Glass Pad", "Pads", 70.0 }, glassPad },
            { { "Tape Strings", "Pads", 84.0 }, tapeStrings },
            { { "Void Texture", "Textures", 100.0 }, voidTexture },
            { { "Sub Bassline", "Bass", 120.0 }, subBassline },
            { { "Reese Pressure", "Bass", 174.0 }, reesePressure },
            { { "808 Glide", "Bass", 140.0 }, glide808 },
            { { "Neon Pluck Arp", "Plucks", 128.0 }, neonPluckArp },
            { { "Nylon Pluck", "Plucks", 96.0 }, nylonPluck },
            { { "Music Box Bells", "Bells", 110.0 }, musicBoxBells },
            { { "Ghost Vox", "Vox", 100.0 }, ghostVox },
            { { "Chiptune Lead", "Leads", 150.0 }, chiptuneLead },
        } };
        return list;
    }
} // namespace

int getNumFactorySamples() { return (int) entries().size(); }

const FactorySampleInfo& getFactorySampleInfo (int index)
{
    return entries()[(size_t) juce::jlimit (0, getNumFactorySamples() - 1, index)].info;
}

SampleData::Ptr createFactorySample (int index)
{
    index = juce::jlimit (0, getNumFactorySamples() - 1, index);

    static std::mutex cacheLock;
    static std::array<SampleData::Ptr, 20> cache;
    const std::lock_guard<std::mutex> lock (cacheLock);
    if (cache[(size_t) index] != nullptr)
        return cache[(size_t) index];

    const auto& entry = entries()[(size_t) index];
    Canvas canvas (entry.info.bpm);
    entry.make (canvas);

    float peak = 0.0f;
    for (size_t i = 0; i < canvas.l.size(); ++i)
        peak = juce::jmax (peak, std::abs (canvas.l[i]), std::abs (canvas.r[i]));
    const float norm = peak > 0.0f ? 0.89f / peak : 1.0f;

    juce::AudioBuffer<float> audio (2, (int) canvas.l.size());
    for (int i = 0; i < audio.getNumSamples(); ++i)
    {
        audio.setSample (0, i, canvas.l[(size_t) i] * norm);
        audio.setSample (1, i, canvas.r[(size_t) i] * norm);
    }

    auto data = makeSampleData (std::move (audio), sr, entry.info.name);
    data->source.kind = SampleSource::Kind::factory;
    data->source.factoryIndex = index;
    data->bpm = entry.info.bpm;
    data->bpmIsKnown = true;
    cache[(size_t) index] = data;
    return data;
}
} // namespace rs
