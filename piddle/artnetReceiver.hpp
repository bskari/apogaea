#pragma once

#include "constants.hpp"
#include <FastLED.h>

// Start the WiFi AP and ArtNet receiver task. Call from loop() after tearing down Bluetooth.
void setupArtnet();

// FreeRTOS task - do not call directly.
void artnetReceiverFunction(void*);

// True when ArtNet packets are actively arriving. Clears after 120s of silence (falls back to audio).
extern volatile bool artnetActive;

// Most recent pixel data received from ArtNet, row-major: index as [strip * LEDS_PER_STRIP + led].
// Strip N corresponds to ArtNet universe N. Allocated by setupArtnet().
extern CRGB* artnetPixels;
