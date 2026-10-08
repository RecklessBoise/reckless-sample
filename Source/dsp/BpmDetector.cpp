#include "BpmDetector.h"

namespace rs
{
namespace
{
    constexpr double minBpm = 60.0, maxBpm = 200.0;

    double autocorr (const std::vector<float>& x, double lag);
} // namespace

std::vector<float> onsetStrength (const juce::AudioBuffer<float>& audio, int hop)
{
        constexpr int order = 10;
        constexpr int size = 1 << order;
        juce::dsp::FFT fft (order);
        juce::dsp::WindowingFunction<float> window (size, juce::dsp::WindowingFunction<float>::hann);

        const int n = audio.getNumSamples();
        const int channels = audio.getNumChannels();
        std::vector<float> frame (size * 2), prev (size / 2 + 1, 0.0f), env;
        env.reserve ((size_t) (n / hop + 1));

        for (int start = 0; start + size <= n; start += hop)
        {
            std::fill (frame.begin(), frame.end(), 0.0f);
            for (int c = 0; c < channels; ++c)
            {
                const auto* src = audio.getReadPointer (c, start);
                for (int i = 0; i < size; ++i)
                    frame[(size_t) i] += src[i] / (float) channels;
            }
            window.multiplyWithWindowingTable (frame.data(), size);
            fft.performFrequencyOnlyForwardTransform (frame.data());

            float flux = 0.0f;
            for (int b = 1; b <= size / 2; ++b)
            {
                const float mag = std::log1p (1000.0f * frame[(size_t) b]);
                flux += std::max (0.0f, mag - prev[(size_t) b]);
                prev[(size_t) b] = mag;
            }
            env.push_back (flux);
        }

        // Remove the slowly varying part so the autocorrelation sees pulses only.
        std::vector<float> out (env.size(), 0.0f);
        for (size_t i = 0; i < env.size(); ++i)
        {
            const size_t lo = i >= 16 ? i - 16 : 0, hi = std::min (env.size(), i + 17);
            float mean = 0.0f;
            for (size_t j = lo; j < hi; ++j)
                mean += env[j];
            out[i] = std::max (0.0f, env[i] - mean / (float) (hi - lo));
        }
        return out;
}

namespace
{
    double autocorr (const std::vector<float>& x, double lag)
    {
        // Linear interpolation lets us evaluate fractional lags.
        const int l0 = (int) lag;
        const double frac = lag - l0;
        double sum = 0.0;
        const size_t n = x.size();
        for (size_t i = 0; i + (size_t) l0 + 1 < n; ++i)
            sum += x[i] * ((1.0 - frac) * x[i + (size_t) l0] + frac * x[i + (size_t) l0 + 1]);
        return sum / (double) n;
    }
} // namespace

double foldBpm (double bpm)
{
    if (bpm <= 0.0)
        return 120.0;
    while (bpm < 70.0)
        bpm *= 2.0;
    while (bpm >= 180.0)
        bpm *= 0.5;
    return bpm;
}

std::optional<double> bpmFromFileName (const juce::String& fileName)
{
    const auto lower = fileName.toLowerCase();
    const int idx = lower.indexOf ("bpm");
    if (idx < 0)
        return std::nullopt;

    auto parseNumberAround = [&] (bool before) -> std::optional<double>
    {
        int i = before ? idx - 1 : idx + 3;
        const int step = before ? -1 : 1;
        // Skip separators between the number and "bpm".
        while (i >= 0 && i < lower.length() && juce::String (" _-.").containsChar (lower[i]))
            i += step;

        int lo = i, hi = i;
        while (lo - 1 >= 0 && (juce::CharacterFunctions::isDigit (lower[lo - 1]) || lower[lo - 1] == '.') && before)
            --lo;
        while (hi + 1 < lower.length() && (juce::CharacterFunctions::isDigit (lower[hi + 1]) || lower[hi + 1] == '.') && ! before)
            ++hi;

        if (i < 0 || i >= lower.length() || ! juce::CharacterFunctions::isDigit (lower[i]))
            return std::nullopt;

        const double v = lower.substring (lo, hi + 1).getDoubleValue();
        if (v >= 40.0 && v <= 250.0)
            return v;
        return std::nullopt;
    };

    if (auto v = parseNumberAround (true))
        return v;
    return parseNumberAround (false);
}

BpmResult detectBpm (const juce::AudioBuffer<float>& audio, double sampleRate, const juce::String& fileName)
{
    if (auto named = bpmFromFileName (fileName))
        return { *named, 1.0f, true };

    BpmResult result;
    constexpr int hop = 256;
    auto env = onsetStrength (audio, hop);
    const double framesPerSecond = sampleRate / hop;
    env.resize (std::min (env.size(), (size_t) (framesPerSecond * 90.0))); // 90 s is plenty for a tempo estimate
    const double minLag = framesPerSecond * 60.0 / maxBpm;
    const double maxLag = framesPerSecond * 60.0 / minBpm;

    if (env.size() < (size_t) (maxLag * 2.5))
        return result;

    // Score each tempo by the autocorrelation at one, two and four beats,
    // weighted by a broad preference centred on 120 BPM.
    const auto scoreFor = [&] (double bpm)
    {
        const double lag = 60.0 * framesPerSecond / bpm;
        const double prior = std::exp (-0.5 * std::pow (std::log2 (bpm / 120.0) / 0.9, 2.0));
        return prior * (autocorr (env, lag) + 0.5 * autocorr (env, lag * 2.0) + 0.25 * autocorr (env, lag * 4.0));
    };

    double bestScore = 0.0, bestLag = 0.0, totalScore = 0.0;
    int count = 0;
    for (double lag = minLag; lag <= maxLag; lag += 0.25)
    {
        const double score = scoreFor (60.0 * framesPerSecond / lag);
        totalScore += score;
        ++count;
        if (score > bestScore)
        {
            bestScore = score;
            bestLag = lag;
        }
    }

    if (bestLag <= 0.0 || bestScore <= 0.0)
        return result;

    double bpm = foldBpm (60.0 * framesPerSecond / bestLag);

    // Loops usually last a whole number of bars. Among the tempos that fit the
    // loop length exactly, take the strongest; keep the free estimate only if no
    // bar-aligned tempo comes close to it.
    const double seconds = audio.getNumSamples() / sampleRate;
    if (seconds >= 1.0 && seconds <= 64.0)
    {
        double gridBpm = 0.0, gridScore = 0.0;
        for (int beats = 4; beats <= 512; beats += 4)
        {
            const double candidate = 60.0 * beats / seconds;
            if (candidate < 70.0)
                continue;
            if (candidate >= 180.0)
                break;
            const double score = scoreFor (candidate);
            const bool close = std::abs (candidate - bpm) / bpm < 0.03;
            if (close || score > gridScore)
            {
                gridScore = close ? std::numeric_limits<double>::max() : score;
                gridBpm = candidate;
            }
        }
        if (gridBpm > 0.0 && gridScore >= 0.5 * scoreFor (bpm))
            bpm = gridBpm;
    }

    result.bpm = std::round (bpm * 100.0) / 100.0;
    result.confidence = (float) juce::jlimit (0.0, 1.0, (bestScore / (totalScore / count) - 1.0) / 4.0);
    return result;
}
} // namespace rs
