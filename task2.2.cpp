#include <iostream>
#include <thread>
#include <chrono>
#include "portaudio.h"
#include "smbPitchShift.cpp"

const int SAMPLING_FREQ = 44100;
const int BUFFER_SIZE = 512;
const int INPUT_CHANNEL_NO = 1;
const int OUTPUT_CHANNEL_NO = 1;
const PaSampleFormat SAMPLE_FORMAT = paFloat32;
const int DURATION = 20; // Duration of the audio stream in seconds

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


int main()
{
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
    float inputBuffer[BUFFER_SIZE];
    float passOutputBuffer[BUFFER_SIZE];
    float pitchOutputBuffer[BUFFER_SIZE];
    std::cout << "Starting audio stream. Press Ctrl+C to stop." << std::endl;
    while(true)
    {
        err = Pa_ReadStream(stream, inputBuffer, BUFFER_SIZE);
        checkErr(err);

        std::thread pitchShiftThread(smbPitchShift, 0.5, BUFFER_SIZE, 1024, 4, SAMPLING_FREQ, inputBuffer, pitchOutputBuffer);
        std::thread simplePassthroughThread([&](){
            for (int i = 0; i < BUFFER_SIZE; ++i)
            {
                passOutputBuffer[i] = inputBuffer[i];
            }
        });

        pitchShiftThread.join();
        simplePassthroughThread.join();

        err = Pa_WriteStream(stream, outputBuffer, BUFFER_SIZE);
        checkErr(err);
    }

    std::cout << "Stopping audio stream." << std::endl;
    
    err = Pa_StopStream(stream);
    checkErr(err);
    err = Pa_CloseStream(stream);
    checkErr(err);
    err = Pa_Terminate();
    checkErr(err);
}