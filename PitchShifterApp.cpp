#include "PitchShifterApp.hpp"

    PitchShifterApp::PitchShifterApp() {
        inputBufferA = new float[BUFFER_SIZE]();
        inputBufferB = new float[BUFFER_SIZE]();
        outputBufferA = new float[BUFFER_SIZE]();
        outputBufferB = new float[BUFFER_SIZE]();

        checkErr(Pa_Initialize());
        checkErr(Pa_OpenDefaultStream(&stream, INPUT_CHANNEL_NO, OUTPUT_CHANNEL_NO, SAMPLE_FORMAT, SAMPLING_FREQ, BUFFER_SIZE, nullptr, nullptr));
    }

    PitchShifterApp::~PitchShifterApp() {
        // Clean up heap arrays safely
        delete[] inputBufferA;
        delete[] inputBufferB;
        delete[] outputBufferA;
        delete[] outputBufferB;
    }

    void checkErr(PaError err) {
        if (err != paNoError) {
            std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    // 1. Worker Thread: Processes Audio Data
    void PitchShifterApp::processingWorker() {
        while (isRunning) {
            if (isBufferAReadyForProcessing) {
                if (isPassthrough) {
                    std::copy(inputBufferA, inputBufferA + BUFFER_SIZE, outputBufferA);
                } else {
                    smbPitchShift(pitchShiftFactor.load(), BUFFER_SIZE, 1024, 4, SAMPLING_FREQ, inputBufferA, outputBufferA);
                }
                isBufferAReadyForProcessing = false;
            }
            else if (isBufferBReadyForProcessing) {
                if (isPassthrough) {
                    std::copy(inputBufferB, inputBufferB + BUFFER_SIZE, outputBufferB);
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

    // 2. Worker Thread: Captures User Commands
    void PitchShifterApp::keyboardWorker() {
        char input;
        std::cout << "\n=== Interactive Pitch Shifter Controls ===\n"
                  << "[s] Start Pitch Shifter\n"
                  << "[p] Passthrough Mode\n"
                  << "[u] Increase Pitch Factor (+0.5)\n"
                  << "[d] Decrease Pitch Factor (-0.5)\n"
                  << "[q] Quit Program\n"
                  << "==========================================\n\n";

        while (isRunning) {
            std::cin >> input;

            switch (input) {
                case 'q':
                    std::cout << "Signal received. Stopping engine on next audio frame cycle...\n";
                    isRunning = false; // Soft trigger: stops loop safely
                    return; 
                case 's':
                    if (isPassthrough) {
                        isPassthrough = false;
                        std::cout << "[State] Pitch Shifter Active. Factor: " << pitchShiftFactor.load() << "\n";
                    }
                    break;
                case 'p':
                    if (!isPassthrough) {
                        isPassthrough = true;
                        std::cout << "[State] Passthrough Mode Active.\n";
                    }
                    break;
                case 'u':
                    if (!isPassthrough) {
                        float current = pitchShiftFactor.load();
                        if (current < 2.0f) {
                            pitchShiftFactor.store(current + 0.5f);
                            std::cout << "[Pitch] Factor increased to: " << pitchShiftFactor.load() << "\n";
                        } else {
                            std::cout << "[Pitch] Factor increased is at max (2.0)"  << "\n";
                        }
                    }
                    break;
                case 'd':
                    if (!isPassthrough) {
                        float current = pitchShiftFactor.load();
                        if (current > 0.5f) {
                            pitchShiftFactor.store(current - 0.5f);
                            std::cout << "[Pitch] Factor decreased to: " << pitchShiftFactor.load() << "\n";
                        }
                    }
                    break;
                default:
                    break;
            }
        }
    }

    void PitchShifterApp::start() {
        checkErr(Pa_StartStream(stream));
        isRunning = true;

        // Spin up workers
        processingThread = std::thread(&PitchShifterApp::processingWorker, this);
        keyboardThread = std::thread(&PitchShifterApp::keyboardWorker, this);

        // Enter the blocker loop directly on the Main Thread
        runAudioLoop();
    }

    void PitchShifterApp::runAudioLoop() {
        bool useBufferA = true;

        // Main execution thread stays entirely here processing stream sequences
        while (isRunning) {
            if (useBufferA) {
                Pa_ReadStream(stream, inputBufferA, BUFFER_SIZE);
                isBufferAReadyForProcessing = true;
                Pa_WriteStream(stream, outputBufferB, BUFFER_SIZE);
            } else {
                Pa_ReadStream(stream, inputBufferB, BUFFER_SIZE);
                isBufferBReadyForProcessing = true;
                Pa_WriteStream(stream, outputBufferA, BUFFER_SIZE);
            }
            useBufferA = !useBufferA;
        }
    }

    // This method is called from MAIN thread ONLY after runAudioLoop completes
    void PitchShifterApp::stop() {
        std::cout << "Cleaning up background worker threads...\n";

        if (processingThread.joinable()) {
            processingThread.join();
        }
        if (keyboardThread.joinable()) {
            keyboardThread.join();
        }

        // Now that no threads are calling ReadStream/WriteStream, it's safe to tear down hardware
        if (stream) {
            Pa_StopStream(stream);
            Pa_CloseStream(stream);
            stream = nullptr;
        }
        Pa_Terminate();
        std::cout << "Engine cleanly terminated with zero active threads.\n";
    }
    