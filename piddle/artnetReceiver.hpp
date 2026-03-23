#pragma once

#include "constants.hpp"

#ifdef USE_ARTNET

#include <FastLED.h>

// Start WiFi and the ArtNet receiver task. Blocks up to 10s for WiFi.
// Call from loop() on Core 1 so the LED display keeps running during connect.
void setupArtnet();

// FreeRTOS task - do not call directly.
void artnetReceiverFunction(void*);

// True when ArtNet packets are arriving. Clears automatically after 5s of silence.
extern volatile bool artnetActive;

// True when ArtNet mode is manually enabled (toggled by 3s button hold).
// When false the display ignores incoming ArtNet data and runs audio mode.
extern volatile bool artnetEnabled;

// Set when signal has been lost and WiFi should be torn down to restore DAC pins.
// Check this from the display task and call teardownArtnet() + reinit the LED driver.
extern volatile bool artnetWifiShutdownNeeded;

// Stops the UDP socket and shuts down WiFi. Call from the display task when
// artnetWifiShutdownNeeded is set, then reinitialise the LED driver.
void teardownArtnet();

// Most recent pixel data received from ArtNet, indexed [strip][led].
// Strip N corresponds to ArtNet universe N (universe offset 0).
// LED 0 is the centre of each strip.
extern CRGB artnetPixels[STRIP_COUNT][LEDS_PER_STRIP];

#endif // USE_ARTNET
