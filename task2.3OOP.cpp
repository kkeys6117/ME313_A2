#include "PitchShifterApp.cpp"

void printDeviceInfo(const PaDeviceInfo *deviceInfo, const char *deviceType)
{
    std::cout << "Default " << deviceType << " Device: " << deviceInfo->name << std::endl;
    std::cout << " Input Channels: " << deviceInfo->maxInputChannels << std::endl;
    std::cout << " Output Channels: " << deviceInfo->maxOutputChannels << std::endl;
    std::cout << " Default Sample Rate: " << deviceInfo->defaultSampleRate << std::endl;
    // You can print more information about the device if needed
    std::cout << std::endl;
}

int main() {
    PitchShifterApp app;

    // 1. Starts threads and runs until 'q' sets isRunning to false
    app.start();

    // 2. Main thread arrives here ONLY after runAudioLoop completes safely
    app.stop(); 

    return 0;
}