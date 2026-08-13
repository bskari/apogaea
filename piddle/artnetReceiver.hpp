#pragma once

#include "constants.hpp"

#if ENABLE_ARTNET

#include <FastLED.h>
#include <freertos/semphr.h>

// Connect to WiFi and start the ArtNet receiver task. Call from loop() after tearing down Bluetooth.
// collectSamplesTask must already be suspended before calling. On ArtNet timeout, the receiver
// task resumes collectSamplesTask, clears *artnetMode, and deletes itself.
void setupArtnet(TaskHandle_t collectSamplesTask, volatile bool* artnetMode);

// FreeRTOS task - do not call directly.
void artnetReceiverFunction(void*);

// True when ArtNet packets are actively arriving. Clears after 120s of silence (falls back to audio).
extern volatile bool artnetActive;

// Most recent pixel data received from ArtNet, row-major: index as [strip * LEDS_PER_STRIP + led].
// Strip N corresponds to ArtNet universe N. Allocated by setupArtnet().
extern CRGB* artnetPixels;

// Mutex that must be held while reading or writing artnetPixels.
extern SemaphoreHandle_t artnetPixelsMutex;

#endif // ENABLE_ARTNET
