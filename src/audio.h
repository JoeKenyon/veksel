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
    int max_input_channels;
    int max_output_channels;
};

struct AudioEngine
{
    PaStream *stream = nullptr;
    std::atomic<float> frequency{440.0f};
    std::atomic<float> volume{0.2f};
    float phase = 0.0f;
    
    ScopeBuffer scope_buffer{4096};
    
    int selected_input_device = -1;
    int selected_output_device = -1;

    static int callback(const void *inputBuffer, void *outputBuffer,
                        unsigned long framesPerBuffer,
                        const PaStreamCallbackTimeInfo *timeInfo,
                        PaStreamCallbackFlags statusFlags,
                        void *userData)
    {
        AudioEngine *engine = static_cast<AudioEngine*>(userData);
        const float *in = static_cast<const float*>(inputBuffer);
        float *out = static_cast<float*>(outputBuffer);
        
        float freq = engine->load_frequency();
        float vol = engine->load_volume();
        float sampleRate = 44100.0f;
        
        for (unsigned long i = 0; i < framesPerBuffer; ++i)
        {
            float sample_to_scope = 0.0f;
            float output_sample = 0.0f;

            if (in != nullptr)
            {
                // grab channel 0 (works for both mono and stereo)
                float left_in = *in++;
                // if input has 2 channels, advance pointer past right channel too
                // (or check your device channel configuration if needed)
                sample_to_scope = left_in;
                output_sample = left_in * vol; 
            }
            else
            {
                output_sample = std::sin(engine->phase * 2.0f * 3.14159265f) * vol;
                sample_to_scope = output_sample;
                
                engine->phase += freq / sampleRate;
                if (engine->phase >= 1.0f)
                {
                    engine->phase -= 1.0f;
                }
            }

            // write out (assuming stereo output speakers)
            *out++ = output_sample;
            *out++ = output_sample;

            engine->scope_buffer.push(sample_to_scope);
        }
        
        return paContinue;
    }

    std::vector<AudioDeviceInfo> get_devices()
    {
        std::vector<AudioDeviceInfo> devices;
        int num_devices = Pa_GetDeviceCount();
        for (int i = 0; i < num_devices; ++i)
        {
            const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
            if (info)
            {
                devices.push_back({i, info->name, info->maxInputChannels, info->maxOutputChannels});
            }
        }
        return devices;
    }

    bool init(int input_id = -1, int output_id = -1)
    {
        PaError err = Pa_Initialize();
        if (err != paNoError)
        {
            std::cerr << "portaudio init error: " << Pa_GetErrorText(err) << "\n";
            return false;
        }

        PaStreamParameters inputParameters;
        PaStreamParameters outputParameters;

        // setup input parameters dynamically
        inputParameters.device = (input_id == -1) ? Pa_GetDefaultInputDevice() : input_id;
        selected_input_device = inputParameters.device;
        
        bool use_input = (inputParameters.device != paNoDevice);
        if (use_input)
        {
            const PaDeviceInfo *inputInfo = Pa_GetDeviceInfo(inputParameters.device);
            // use up to 2 channels, but fall back to max available if device only supports 1 (mono)
            inputParameters.channelCount = (inputInfo && inputInfo->maxInputChannels < 2) ? inputInfo->maxInputChannels : 2;
            inputParameters.sampleFormat = paFloat32;
            inputParameters.suggestedLatency = inputInfo ? inputInfo->defaultLowInputLatency : 0.01;
            inputParameters.hostApiSpecificStreamInfo = nullptr;
        }

        // setup output parameters dynamically
        outputParameters.device = (output_id == -1) ? Pa_GetDefaultOutputDevice() : output_id;
        selected_output_device = outputParameters.device;
        
        if (outputParameters.device == paNoDevice)
        {
            std::cerr << "no output device found\n";
            return false;
        }
        
        const PaDeviceInfo *outputInfo = Pa_GetDeviceInfo(outputParameters.device);
        outputParameters.channelCount = (outputInfo && outputInfo->maxOutputChannels < 2) ? outputInfo->maxOutputChannels : 2;
        outputParameters.sampleFormat = paFloat32;
        outputParameters.suggestedLatency = outputInfo ? outputInfo->defaultLowOutputLatency : 0.01;
        outputParameters.hostApiSpecificStreamInfo = nullptr;

        err = Pa_OpenStream(
            &stream,
            use_input ? &inputParameters : nullptr,
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

    void restart(int input_id, int output_id)
    {
        if (stream)
        {
            Pa_StopStream(stream);
            Pa_CloseStream(stream);
            stream = nullptr;
        }
        init(input_id, output_id);
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