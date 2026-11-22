#pragma once

#include "constants.hpp"

#include <FastLED.h>
#include <freertos/semphr.h>

// Connect to WiFi and start the ArtNet receiver task. Call from loop() after tearing down Bluetooth.
// collectSamplesTask must already be suspended before calling. On ArtNet timeout, the receiver
// task resumes collectSamplesTask, clears *artnetMode, and deletes itself.
void setupArtNet(TaskHandle_t collectSamplesTask, volatile bool* artNetMode);

// FreeRTOS task - do not call directly.
void artNetReceiverFunction(void*);

// True when ArtNet packets are actively arriving. Clears after 120s of silence (falls back to audio).
extern volatile bool artNetActive;

// Most recent pixel data received from ArtNet, row-major: index as [strip * LEDS_PER_STRIP + led].
// Strip N corresponds to ArtNet universe N. Allocated by setupArtNet().
extern CRGB* artNetPixels;

// Mutex that must be held while reading or writing artNetPixels.
extern SemaphoreHandle_t artNetPixelsMutex;

// Call with artNetPixelsMutex held. Returns true when a frame should be drawn: every universe has
// arrived since the last draw, or a partial frame has waited long enough. Resets the tracking, so
// it returns true once per frame.
bool artNetFrameReadyLocked();
