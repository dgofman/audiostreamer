#pragma once

#include <vector>   // std::vector
#include <complex>  // std::complex

namespace denoise_lite {

class DenoiseLite {
public:
    DenoiseLite();
    ~DenoiseLite();

    void Reset();
    void ProcessFrame(float* frame, size_t frameSize);
    bool IsVoiceActive() const;

private:
    static constexpr size_t kBands = 22;
    float noiseEstimate[kBands];
    bool noiseInitialized;
    float vadProb;

    void ComputeFFT(const float* frame, size_t frameSize, std::vector<std::complex<float>>& fftOut);
    void ComputeBandEnergy(const std::vector<std::complex<float>>& fftOut, size_t frameSize, std::vector<float>& bandEnergy);
    void UpdateNoiseEstimate(const std::vector<float>& bandEnergy);
    void ComputeGain(const std::vector<float>& bandEnergy, std::vector<float>& gain);
    void ApplyGain(float* frame, size_t frameSize, const std::vector<float>& gain);
    float ComputeVAD(const std::vector<float>& bandEnergy);
};

} // namespace denoise_lite
