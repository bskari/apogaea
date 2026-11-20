#ifndef SPECTRUM_ANALYZER_HPP
#define SPECTRUM_ANALYZER_HPP

#include <stdint.h>

void displaySpectrumAnalyzer(uint8_t brightness_p, uint8_t palette, bool normalizeBands, uint8_t sensitivity_p, uint8_t speed_p, uint16_t rgb, bool showConverterLeds, uint8_t skipStrip);
void setupSpectrumAnalyzer();
void collectSamples();

// Write mono samples from a Bluetooth A2DP stream into the shared circular buffer.
// stereoData: interleaved L/R int16 pairs at 44100 Hz
// frameCount: number of stereo frames (length in bytes / 4)
void writeBtSamples(const uint8_t* stereoData, uint32_t length);

// Set to true while a Bluetooth audio source is connected.
// When true, displaySpectrumAnalyzer skips the I2S sign-fix and the mic task is suspended.
extern volatile bool bluetoothActive;

// Ask the display task to turn every LED off and show immediately (skipping the shift-down).
// Safe to call from another task; handled at the start of the next displaySpectrumAnalyzer call.
void requestBlankLeds();

#endif
