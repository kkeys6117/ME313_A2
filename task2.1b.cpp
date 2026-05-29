#include <iostream>
#include <algorithm>
#include "portaudio.h"
#include "smbPitchShift.cpp"

// --- 1. PitchShifter Class ---
class PitchShifter {
private:
    float factor;
    int fftFrameSize;
    int osamp;
    float sampleRate;

public:
    PitchShifter(float defaultFactor = 2.0f, float rate = 44100.0f) 
        : factor(defaultFactor), fftFrameSize(1024), osamp(32), sampleRate(rate) {}

    void process(float* input, float* output, int bufferSize) {
        // Enforce the assignment parameters inside the object wrapper
        smbPitchShift(factor, bufferSize, fftFrameSize, osamp, sampleRate, input, output);
    }

    void setFactor(float newFactor) { factor = newFactor; }
    float getFactor() const { return factor; }
};

// --- 2. AudioHardware Class ---
class AudioHardware {
private:
    PaStream* stream;
    int sampleRate;
    int bufferSize;

public:
    AudioHardware(int rate = 44100, int size = 512) 
        : stream(nullptr), sampleRate(rate), bufferSize(size) {}

    ~AudioHardware() { stop(); }

    void start() {
        Pa_Initialize();
        Pa_OpenDefaultStream(&stream, 1, 1, paFloat32, sampleRate, bufferSize, nullptr, nullptr);
        Pa_StartStream(stream);
    }

    void stop() {
        if (stream) {
            Pa_StopStream(stream);
            Pa_CloseStream(stream);
            Pa_Terminate();
            stream = nullptr;
        }
    }

    void read(float* buffer) { Pa_ReadStream(stream, buffer, bufferSize); }
    void write(float* buffer) { Pa_WriteStream(stream, buffer, bufferSize); }
};

// --- Execution for Task 2.1b ---
int main() {
    AudioHardware audio(44100, 512);
    PitchShifter shifter(2.0f, 44100.0f); // Octave up

    float inputBuffer[512] = {0};
    float outputBuffer[512] = {0};

    audio.start();
    std::cout << "Single-threaded OOP Pitch Shifter running...\n";

    // Simple procedural loop using objects
    while(true) { 
        audio.read(inputBuffer);
        shifter.process(inputBuffer, outputBuffer, 512);
        audio.write(outputBuffer);
    }

    audio.stop();
    return 0;
}
