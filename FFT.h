/*
  ==============================================================================

    FFT.h
    Created: 14 Jan 2023 6:50:14pm
    Author:  Oriol Freixa (TeMuFra)

  ==============================================================================
*/

#pragma once

#include <algorithm>

#if defined(_WIN32) || defined(__linux__)
#if __has_include(<ipp/ipps.h>) && __has_include(<stdint.h>)
    #include <ipp/ipps.h>
#endif
    #if __has_include(<ipps.h>) && __has_include(<stdint.h>)
    #include <ipps.h>
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

#else
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

#endif
