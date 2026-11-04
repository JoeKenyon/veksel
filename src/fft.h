#pragma once
#include <vector>
#include <cmath>
#include <complex>

class SpectrumAnalyzer
{
public:
    SpectrumAnalyzer(size_t fft_size) : size(fft_size), magnitudes(fft_size / 2, 0.0f)
    {
    }

    // basic Cooley-Tukey in-place iterative FFT
    void compute(const float *time_domain_data, float sample_rate)
    {
        std::vector<std::complex<float>> buffer(size);
        
        // apply hann window and copy to complex buffer
        for (size_t i = 0; i < size; ++i)
        {
            float window = 0.5f * (1.0f - std::cos(2.0f * 3.14159265f * i / (size - 1)));
            buffer[i] = std::complex<float>(time_domain_data[i] * window, 0.0f);
        }

        // bit-reversal sorting
        size_t j = 0;
        for (size_t i = 0; i < size; ++i)
        {
            if (i < j)
            {
                std::swap(buffer[i], buffer[j]);
            }
            
            size_t bit = size >> 1;
            for (; bit & j; bit >>= 1)
            {
                j &= ~bit;
            }
            j |= bit;
        }

        // computation
        for (size_t len = 2; len <= size; len <<= 1)
        {
            float ang = -2.0f * 3.14159265f / len;
            std::complex<float> wlen(std::cos(ang), std::sin(ang));
            
            for (size_t i = 0; i < size; i += len)
            {
                std::complex<float> w(1.0f, 0.0f);
                for (size_t k = 0; k < len / 2; ++k)
                {
                    std::complex<float> u = buffer[i + k];
                    std::complex<float> v = buffer[i + k + len / 2] * w;
                    buffer[i + k] = u + v;
                    buffer[i + k + len / 2] = u - v;
                    w *= wlen;
                }
            }
        }

        // compute magnitudes for the positive frequencies
        for (size_t i = 0; i < size / 2; ++i)
        {
            float mag = std::abs(buffer[i]) / (size / 2);
            // convert to db scale with a floor to avoid log(0)
            float db = 20.0f * std::log10(mag + 1e-6f);
            
            // normalize roughly between 0.0 and 1.0 for plotting
            magnitudes[i] = (db + 60.0f) / 60.0f;
            if (magnitudes[i] < 0.0f) magnitudes[i] = 0.0f;
            if (magnitudes[i] > 1.0f) magnitudes[i] = 1.0f;
        }
    }

    const float* get_magnitudes() const
    {
        return magnitudes.data();
    }

    size_t get_bin_count() const
    {
        return size / 2;
    }

private:
    size_t size;
    std::vector<float> magnitudes;
};