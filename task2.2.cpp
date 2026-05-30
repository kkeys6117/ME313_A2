#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include "portaudio.h"
#include "smbPitchShift.hpp"

using namespace std;

const int SAMPLING_FREQ = 44100;
const int BUFFER_SIZE = 1024;
const int INPUT_CHANNEL_NO = 1;
const int OUTPUT_CHANNEL_NO = 1;
const PaSampleFormat SAMPLE_FORMAT = paFloat32;
const int DURATION = 20; // Duration of the audio stream in seconds

// Two distinct processing areas
float inputBufferA[BUFFER_SIZE] = {0};
float inputBufferB[BUFFER_SIZE] = {0};
float outputBufferA[BUFFER_SIZE] = {0};
float outputBufferB[BUFFER_SIZE] = {0};

// Atomic flags for lock-free coordination
std::atomic<bool> isBufferAReadyForProcessing(false);
std::atomic<bool> isBufferBReadyForProcessing(false);
std::atomic<bool> isRunning(true);

void processingWorker() {
    while (isRunning) {
        // Look for work in Buffer A
        if (isBufferAReadyForProcessing) {
            smbPitchShift(2.0f, BUFFER_SIZE, 1024, 4, SAMPLING_FREQ, inputBufferA, outputBufferA);
            isBufferAReadyForProcessing = false; // Finished crunching A
        }
        // Look for work in Buffer B
        else if (isBufferBReadyForProcessing) {
            smbPitchShift(2.0f, BUFFER_SIZE, 1024, 4, SAMPLING_FREQ, inputBufferB, outputBufferB);
            isBufferBReadyForProcessing = false; // Finished crunching B
        }
        else {
            // Very brief sleep to prevent 100% CPU usage while waiting
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    }
}

void printDeviceInfo(const PaDeviceInfo *deviceInfo, const char *deviceType)
{
    std::cout << "Default " << deviceType << " Device: " << deviceInfo->name << std::endl;
    std::cout << " Input Channels: " << deviceInfo->maxInputChannels << std::endl;
    std::cout << " Output Channels: " << deviceInfo->maxOutputChannels << std::endl;
    std::cout << " Default Sample Rate: " << deviceInfo->defaultSampleRate << std::endl;
    // You can print more information about the device if needed
    std::cout << std::endl;
}
static void checkErr(PaError err)
{
    if (err != paNoError)
    {
        printf("PortAudio error: %s\n", Pa_GetErrorText(err));
        exit(EXIT_FAILURE);
    }
}

int main() {
    // ... PortAudio Initialization & Stream Setup ...

    PaStream* stream;
    PaError err;

    // initialising portAudio, checking input and output audio devices
    err = Pa_Initialize();
    checkErr(err);
    int defaultInputDevice = Pa_GetDefaultInputDevice();
    int defaultOutputDevice = Pa_GetDefaultOutputDevice();
    if (defaultInputDevice == paNoDevice || defaultOutputDevice == paNoDevice)
    {
        std::cerr << "No default input or output device found." << std::endl;
        Pa_Terminate();
        return -1;
    }
    std::cout << "Available audio devices:" << std::endl;
    for (int i = 0; i < Pa_GetDeviceCount(); ++i)
    {
        const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(i);
        if (i == defaultInputDevice)
        {
            printDeviceInfo(deviceInfo, "Input");
        }
        if (i == defaultOutputDevice)
        {
            printDeviceInfo(deviceInfo, "Output");
        }
    }

    err = Pa_OpenDefaultStream(&stream, INPUT_CHANNEL_NO, OUTPUT_CHANNEL_NO, SAMPLE_FORMAT, SAMPLING_FREQ, BUFFER_SIZE, nullptr, nullptr);
    checkErr(err);

    err = Pa_StartStream(stream);
    checkErr(err);

    // 2. Start the processing thread
    std::thread processingThread(processingWorker);
    bool useBufferA = true; // Alternates every block

    std::cout << "Lock-Free Multi-threaded Pitch Shifting running...\n";

    while (isRunning) {
        if (useBufferA) {
            // 1. Read mic data directly into space A
            Pa_ReadStream(stream, inputBufferA, BUFFER_SIZE);
            
            // 2. Signal worker thread that space A is ready to be pitch-shifted
            isBufferAReadyForProcessing = true;

            // 3. Play back the data that was already processed in space B during the LAST cycle
            Pa_WriteStream(stream, outputBufferB, BUFFER_SIZE);
        } 
        else {
            // 1. Read mic data into space B
            Pa_ReadStream(stream, inputBufferB, BUFFER_SIZE);
            
            // 2. Signal worker thread that space B is ready
            isBufferBReadyForProcessing = true;

            // 3. Play back the data processed in space A
            Pa_WriteStream(stream, outputBufferA, BUFFER_SIZE);
        }

        useBufferA = !useBufferA; // Swap buffers for the next block
    }

    // --- Cleanup ---
    isRunning = false;
    if (processingThread.joinable()) processingThread.join();
    // ... PortAudio Close & Terminate ...
    return 0;
}