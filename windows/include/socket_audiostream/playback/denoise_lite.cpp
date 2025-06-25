#define _USE_MATH_DEFINES // M_PI
#include <numeric>        // std::accumulate
#include <vector>         // std::vector
#include <algorithm>      // std::clamp

#include "denoise_lite.h"

namespace denoise_lite
{

    DenoiseLite::DenoiseLite() : noiseInitialized(false),
                                 vadProb(0.0f)
    {
        std::fill(std::begin(noiseEstimate), std::end(noiseEstimate), 0.0f);
    }

    DenoiseLite::~DenoiseLite() = default;

    void DenoiseLite::Reset()
    {
        noiseInitialized = false;
        vadProb = 0.0f;
        std::fill(std::begin(noiseEstimate), std::end(noiseEstimate), 0.0f);
    }

    void DenoiseLite::ComputeFFT(const float *frame, size_t frameSize, std::vector<std::complex<float>> &fftOut)
    {
        // Simple DFT for example — replace with real FFT later (kissFFT, fftw)
        fftOut.resize(frameSize / 2 + 1);
        for (size_t k = 0; k < fftOut.size(); ++k)
        {
            std::complex<float> sum(0.0f, 0.0f);
            for (size_t n = 0; n < frameSize; ++n)
            {
                float angle = -2.0f * static_cast<float>(M_PI) * k * n / frameSize;
                sum += std::polar(frame[n], angle);
            }
            fftOut[k] = sum;
        }
    }

    void DenoiseLite::ComputeBandEnergy(const std::vector<std::complex<float>> &fftOut, size_t frameSize, std::vector<float> &bandEnergy)
    {
        bandEnergy.resize(kBands);
        std::fill(bandEnergy.begin(), bandEnergy.end(), 0.0f);

        size_t binsPerBand = fftOut.size() / kBands;
        for (size_t b = 0; b < kBands; ++b)
        {
            size_t start = b * binsPerBand;
            size_t end = std::min(start + binsPerBand, fftOut.size());
            float sum = 0.0f;
            for (size_t i = start; i < end; ++i)
            {
                sum += std::norm(fftOut[i]);
            }
            bandEnergy[b] = sum / (end - start + 1);
        }
    }

    void DenoiseLite::UpdateNoiseEstimate(const std::vector<float> &bandEnergy)
    {
        if (!noiseInitialized)
        {
            for (size_t i = 0; i < kBands; ++i)
            {
                noiseEstimate[i] = bandEnergy[i];
            }
            noiseInitialized = true;
        }
        else
        {
            for (size_t i = 0; i < kBands; ++i)
            {
                noiseEstimate[i] = 0.95f * noiseEstimate[i] + 0.05f * bandEnergy[i];
            }
        }
    }

    void DenoiseLite::ComputeGain(const std::vector<float> &bandEnergy, std::vector<float> &gain)
    {
        gain.resize(kBands);
        for (size_t i = 0; i < kBands; ++i)
        {
            float snr = bandEnergy[i] / (noiseEstimate[i] + 1e-8f);
            float g = snr / (snr + 1.0f);
            gain[i] = std::clamp(g, 0.1f, 1.0f); // avoid too much suppression
        }
    }

    void DenoiseLite::ApplyGain(float *frame, size_t frameSize, const std::vector<float> &gain)
    {
        // Simple broadband apply (could inverse FFT for per-band):
        float avgGain = std::accumulate(gain.begin(), gain.end(), 0.0f) / gain.size();
        for (size_t i = 0; i < frameSize; ++i)
        {
            frame[i] *= avgGain;
        }
    }

    float DenoiseLite::ComputeVAD(const std::vector<float> &bandEnergy)
    {
        float totalEnergy = std::accumulate(bandEnergy.begin(), bandEnergy.end(), 0.0f);
        float noiseFloor = std::accumulate(noiseEstimate, noiseEstimate + kBands, 0.0f);
        float snr = totalEnergy / (noiseFloor + 1e-8f);
        float vad = snr / (snr + 1.0f);
        vadProb = 0.95f * vadProb + 0.05f * vad; // smooth
        return vadProb;
    }

    void DenoiseLite::ProcessFrame(float *frame, size_t frameSize)
    {
        std::vector<std::complex<float>> fftOut;
        std::vector<float> bandEnergy;
        std::vector<float> gain;

        ComputeFFT(frame, frameSize, fftOut);
        ComputeBandEnergy(fftOut, frameSize, bandEnergy);
        UpdateNoiseEstimate(bandEnergy);
        ComputeGain(bandEnergy, gain);
        ApplyGain(frame, frameSize, gain);
        ComputeVAD(bandEnergy);
    }

    bool DenoiseLite::IsVoiceActive() const
    {
        return vadProb > 0.5f; // simple threshold
    }

} // namespace denoise_lite
