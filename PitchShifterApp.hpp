#ifndef PITCHSHIFTERAPP_HPP
#define PITCHSHIFTERAPP_HPP

#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <algorithm> // For std::copy
#include "portaudio.h"
#include "smbPitchShift.hpp"

class PitchShifterApp {
private:   
    const int SAMPLING_FREQ = 44100;
    const int BUFFER_SIZE = 1024;
    const int INPUT_CHANNEL_NO = 1;
    const int OUTPUT_CHANNEL_NO = 1;
    const PaSampleFormat SAMPLE_FORMAT = paFloat32;

    float* inputBufferA;
    float* inputBufferB;
    float* outputBufferA;
    float* outputBufferB;

    std::atomic<bool> isBufferAReadyForProcessing{false};
    std::atomic<bool> isBufferBReadyForProcessing{false};
    std::atomic<bool> isRunning{false};
    std::atomic<bool> isPassthrough{false};
    std::atomic<float> pitchShiftFactor{1.0f};

    std::thread processingThread;
    std::thread keyboardThread;
    PaStream* stream = nullptr;

    public:
    PitchShifterApp();
    ~PitchShifterApp();
    void checkErr(PaError err);
    void processingWorker();
    void keyboardWorker();
    void runAudioLoop();
    void start();
    void stop();
};

#endif // PITCHSHIFTERAPP_HPP
