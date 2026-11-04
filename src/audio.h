// inside audio.h
#pragma once
#include <portaudio.h>
#include <cmath>
#include <vector>
#include <atomic>
#include <iostream>
#include <string>

class ScopeBuffer
{
public:
    ScopeBuffer(size_t cap) : capacity(cap), buffer(cap, 0.0f), write_pos(0) {}

    void push(float sample)
    {
        size_t pos = write_pos.load(std::memory_order_relaxed);
        buffer[pos] = sample;
        write_pos.store((pos + 1) % capacity, std::memory_order_release);
    }

    size_t read_latest(float *dest, size_t max_samples)
    {
        size_t current_write = write_pos.load(std::memory_order_acquire);
        size_t to_read = (max_samples < capacity) ? max_samples : capacity;

        for (size_t i = 0; i < to_read; ++i)
        {
            size_t idx = (current_write + capacity - to_read + i) % capacity;
            dest[i] = buffer[idx];
        }
        
        return to_read;
    }

private:
    size_t capacity;
    std::vector<float> buffer;
    std::atomic<size_t> write_pos;
};

struct AudioDeviceInfo
{
    int id;
    std::string name;
    int max_output_channels;
    int max_input_channels;
};

struct AudioEngine
{
    PaStream *stream = nullptr;
    std::atomic<float> frequency{440.0f};
    std::atomic<float> volume{0.2f};
    float phase = 0.0f;
    
    ScopeBuffer scope_buffer{4096};
    int selected_output_device = -1;

    static int callback(const void *inputBuffer, void *outputBuffer,
                        unsigned long framesPerBuffer,
                        const PaStreamCallbackTimeInfo *timeInfo,
                        PaStreamCallbackFlags statusFlags,
                        void *userData)
    {
        AudioEngine *engine = static_cast<AudioEngine*>(userData);
        float *out = static_cast<float*>(outputBuffer);
        
        float freq = engine->load_frequency();
        float vol = engine->load_volume();
        float sampleRate = 44100.0f;
        
        for (unsigned long i = 0; i < framesPerBuffer; ++i)
        {
            float sample = std::sin(engine->phase * 2.0f * 3.14159265f) * vol;
            
            *out++ = sample;
            *out++ = sample;
            
            engine->scope_buffer.push(sample);
            
            engine->phase += freq / sampleRate;
            if (engine->phase >= 1.0f)
            {
                engine->phase -= 1.0f;
            }
        }
        
        return paContinue;
    }

    std::vector<AudioDeviceInfo> get_output_devices()
    {
        std::vector<AudioDeviceInfo> devices;
        int num_devices = Pa_GetDeviceCount();
        for (int i = 0; i < num_devices; ++i)
        {
            const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
            if (info && info->maxOutputChannels > 0)
            {
                devices.push_back({i, info->name, info->maxOutputChannels, info->maxInputChannels});
            }
        }
        return devices;
    }

    bool init(int device_id = -1)
    {
        PaError err = Pa_Initialize();
        if (err != paNoError)
        {
            std::cerr << "portaudio init error: " << Pa_GetErrorText(err) << "\n";
            return false;
        }

        PaStreamParameters outputParameters;
        outputParameters.device = (device_id == -1) ? Pa_GetDefaultOutputDevice() : device_id;
        if (outputParameters.device == paNoDevice)
        {
            std::cerr << "no output device found\n";
            return false;
        }
        
        selected_output_device = outputParameters.device;
        outputParameters.channelCount = 2;
        outputParameters.sampleFormat = paFloat32;
        outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
        outputParameters.hostApiSpecificStreamInfo = nullptr;

        err = Pa_OpenStream(
            &stream,
            nullptr,
            &outputParameters,
            44100,
            256,
            paClipOff,
            callback,
            this
        );

        if (err != paNoError)
        {
            std::cerr << "portaudio open stream error: " << Pa_GetErrorText(err) << "\n";
            return false;
        }
        
        err = Pa_StartStream(stream);
        if (err != paNoError)
        {
            std::cerr << "portaudio start stream error: " << Pa_GetErrorText(err) << "\n";
            return false;
        }
        
        return true;
    }

    void restart(int device_id)
    {
        if (stream)
        {
            Pa_StopStream(stream);
            Pa_CloseStream(stream);
            stream = nullptr;
        }
        init(device_id);
    }

    void shutdown()
    {
        if (stream)
        {
            Pa_StopStream(stream);
            Pa_CloseStream(stream);
            stream = nullptr;
        }
        Pa_Terminate();
    }

    void set_frequency(float f) { frequency.store(f); }
    float load_frequency() const { return frequency.load(); }
    void set_volume(float v) { volume.store(v); }
    float load_volume() const { return volume.load(); }
};