/*
  ==============================================================================

    FFT.h
    Created: 14 Jan 2023 6:50:14pm
    Author:  Oriol Freixa (TeMuFra)

  ==============================================================================
*/

#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#if defined(TMF_ADDITIVE_SYNTH_HAS_IPP)
#if __has_include(<ipp/ipps.h>)
    #include <ipp/ipps.h>
#elif __has_include(<ipps.h>)
    #include <ipps.h>
#else
    #error "TMF_ADDITIVE_SYNTH_HAS_IPP is set, but IPP headers were not found"
#endif

class FourierTransform
{
public:
    FourierTransform(int bits) : size_(1 << bits) {
        int spec_size = 0;
        int spec_buffer_size = 0;
        int buffer_size = 0;
        ippsFFTGetSize_R_32f (bits, IPP_FFT_NODIV_BY_ANY, ippAlgHintNone, &spec_size, &spec_buffer_size, &buffer_size);

        spec_ = std::make_unique<Ipp8u[]>(spec_size);
        spec_buffer_ = std::make_unique <Ipp8u[]>(spec_buffer_size);
        buffer_ = std::make_unique<Ipp8u[]>(buffer_size);

        ippsFFTInit_R_32f (&ipp_specs_, bits, IPP_FFT_NODIV_BY_ANY, ippAlgHintNone, spec_.get(), spec_buffer_.get());
    }

    void transformRealForward(float* data) {
        data[size_] = 0.0f;
        ippsFFTFwd_RToPerm_32f_I((Ipp32f*)data, ipp_specs_, buffer_.get());
        data[size_] = data[1];
        data[size_ + 1] = 0.0f;
        data[1] = 0.0f;
    }

    void transformRealInverse(float* data) {
        data[1] = 0; // Should be data[size_], but we won't be using that
        ippsFFTInv_PermToR_32f_I((Ipp32f*)data, ipp_specs_, buffer_.get());
        // memset(data + size_, 0, size_ * sizeof(float));
    }

private:
    int size_;
    IppsFFTSpec_R_32f* ipp_specs_;
    std::unique_ptr<Ipp8u[]> spec_;
    std::unique_ptr<Ipp8u[]> spec_buffer_;
    std::unique_ptr<Ipp8u[]> buffer_;

    JUCE_LEAK_DETECTOR(FourierTransform)
};

#elif defined(__APPLE__)
#define VIMAGE_H
#include <Accelerate/Accelerate.h>

class FourierTransform {
public:
    FourierTransform(vDSP_Length bits)
        : setup_(vDSP_create_fftsetup(bits, 2)),
          bits_(bits),
          size_(1 << bits),
          scratch_(std::make_unique<float[]>(size_))
    {
    }

    FourierTransform(FourierTransform&& fft)
        : setup_(vDSP_create_fftsetup(fft.bits_, 2)),
          bits_(fft.bits_),
          size_(fft.size_),
          scratch_(std::make_unique<float[]>(size_))
    {
    }

    ~FourierTransform() {
        vDSP_destroy_fftsetup(setup_);
    }

    void transformRealForward(float* data) {
        static const float kMult = 0.5f;
        std::copy_n(data, static_cast<size_t> (size_), scratch_.get());

        DSPSplitComplex split = { scratch_.get(), scratch_.get() + 1 };
        vDSP_fft_zrip(setup_, &split, 2, bits_, kFFTDirection_Forward);
        vDSP_vsmul(scratch_.get(), 1, &kMult, scratch_.get(), 1, size_);

        scratch_[1] = 0.0f;
        std::copy_n(scratch_.get(), static_cast<size_t> (size_), data);
    }

    void transformRealInverse(float* data) {
        std::copy_n(data, static_cast<size_t> (size_), scratch_.get());
        scratch_[1] = 0.0f;

        DSPSplitComplex split = { scratch_.get(), scratch_.get() + 1 };
        vDSP_fft_zrip(setup_, &split, 2, bits_, kFFTDirection_Inverse);

        std::copy_n(scratch_.get(), static_cast<size_t> (size_), data);
    }

private:
    FFTSetup setup_;
    vDSP_Length bits_;
    vDSP_Length size_;
    std::unique_ptr<float[]> scratch_;

    JUCE_LEAK_DETECTOR(FourierTransform)
  };

#else

class FourierTransform {
public:
    FourierTransform(int bits)
        : size_(1 << bits),
          scratch_(static_cast<size_t> (size_))
    {
    }

    void transformRealForward(float* data) {
        constexpr double twoPi = 6.283185307179586476925286766559;

        std::copy_n(data, static_cast<size_t> (size_), scratch_.begin());

        data[0] = 0.0f;
        for (int n = 0; n < size_; ++n)
            data[0] += scratch_[static_cast<size_t> (n)];

        data[1] = 0.0f;
        for (int n = 0; n < size_; ++n)
            data[1] += scratch_[static_cast<size_t> (n)] * (n % 2 == 0 ? 1.0f : -1.0f);

        for (int k = 1; k < size_ / 2; ++k) {
            auto real = 0.0;
            auto imag = 0.0;
            for (int n = 0; n < size_; ++n) {
                const auto phase = twoPi * static_cast<double> (k * n) / static_cast<double> (size_);
                real += static_cast<double> (scratch_[static_cast<size_t> (n)]) * std::cos (phase);
                imag -= static_cast<double> (scratch_[static_cast<size_t> (n)]) * std::sin (phase);
            }

            data[2 * k] = static_cast<float> (real);
            data[2 * k + 1] = static_cast<float> (imag);
        }
    }

    void transformRealInverse(float* data) {
        constexpr double twoPi = 6.283185307179586476925286766559;

        for (int n = 0; n < size_; ++n) {
            auto sample = static_cast<double> (data[0]);
            sample += static_cast<double> (data[1]) * (n % 2 == 0 ? 1.0 : -1.0);

            for (int k = 1; k < size_ / 2; ++k) {
                const auto phase = twoPi * static_cast<double> (k * n) / static_cast<double> (size_);
                const auto real = static_cast<double> (data[2 * k]);
                const auto imag = static_cast<double> (data[2 * k + 1]);
                sample += 2.0 * (real * std::cos (phase) - imag * std::sin (phase));
            }

            scratch_[static_cast<size_t> (n)] = static_cast<float> (sample);
        }

        std::copy_n(scratch_.begin(), static_cast<size_t> (size_), data);
    }

private:
    int size_;
    std::vector<float> scratch_;

    JUCE_LEAK_DETECTOR(FourierTransform)
};

#endif
