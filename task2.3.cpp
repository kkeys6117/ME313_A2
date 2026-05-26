#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include "portaudio.h"
#include "smbPitchShift.cpp"

using namespace std;

const int SAMPLING_FREQ = 44100;
const int BUFFER_SIZE = 512;
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

std::atomic<bool> isPassthrough(false); // true = passthrough, false = pitch shift
std::atomic<float> pitchShiftFactor(1.0f); // default pitch multiplier

void processingWorker() {
    while (isRunning) {
        if (isBufferAReadyForProcessing) {
            if (isPassthrough) {
                // Passthrough mode: Direct copy without processing
                std::copy(std::begin(inputBufferA), std::end(inputBufferA), std::begin(outputBufferA));
            } else {
                // Read the dynamic atomic factor instead of the hardcoded 2.0f
                smbPitchShift(pitchShiftFactor.load(), BUFFER_SIZE, 1024, 4, SAMPLING_FREQ, inputBufferA, outputBufferA);
            }
            isBufferAReadyForProcessing = false;
        }
        else if (isBufferBReadyForProcessing) {
            if (isPassthrough) {
                std::copy(std::begin(inputBufferB), std::end(inputBufferB), std::begin(outputBufferB));
            } else {
                smbPitchShift(pitchShiftFactor.load(), BUFFER_SIZE, 1024, 4, SAMPLING_FREQ, inputBufferB, outputBufferB);
            }
            isBufferBReadyForProcessing = false;
        }
        else {
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

void keyboardInput() {
    char input;
    std::cout << "\n=== Controls ===\n"
              << "[s] Start Pitch Shifter\n"
              << "[p] Passthrough Mode\n"
              << "[u] Increase Pitch (+0.5)\n"
              << "[d] Decrease Pitch (-0.5)\n"
              << "[q] Quit Program\n"
              << "================\n\n";

    while (isRunning) {
        std::cin >> input; // Waits for input + Enter

        switch (input) {
            case 'q':
                std::cout << "Exiting program...\n";
                isRunning = false;
                break;

            case 's':
                if (isPassthrough) {
                    isPassthrough = false;
                    std::cout << "[Mode] Pitch Shifter Enabled (Factor: " << pitchShiftFactor.load() << ")\n";
                }
                break;

            case 'p':
                if (!isPassthrough) {
                    isPassthrough = true;
                    std::cout << "[Mode] Passthrough Enabled\n";
                }
                break;

            case 'u':
                if (isPassthrough) {
                    std::cout << "[Warning] Cannot change pitch while in Passthrough mode.\n";
                } else {
                    // Fetch current value, add 0.5, and update atomically
                    float current = pitchShiftFactor.load();
                    pitchShiftFactor.store(current + 0.5f);
                    std::cout << "[Pitch] Increased to: " << pitchShiftFactor.load() << "\n";
                }
                break;

            case 'd':
                if (isPassthrough) {
                    std::cout << "[Warning] Cannot change pitch while in Passthrough mode.\n";
                } else {
                    float current = pitchShiftFactor.load();
                    // Prevent pitch factor from dropping to 0 or negative values if necessary
                    if (current > 0.5f) {
                        pitchShiftFactor.store(current - 0.5f);
                        std::cout << "[Pitch] Decreased to: " << pitchShiftFactor.load() << "\n";
                    } else {
                        std::cout << "[Warning] Pitch factor cannot drop below 0.5.\n";
                    }
                }
                break;

            default:
                std::cout << "Unknown command: " << input << "\n";
                break;
        }
    }
}

int main() {
    // PortAudio Initialization & Stream Setup

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
    std::thread keyboardThread(keyboardInput);
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
    if (keyboardThread.joinable()) keyboardThread.join();
    // ... PortAudio Close & Terminate ...
    return 0;
}