#ifndef SPECTRUM_ANALYZER_HPP
#define SPECTRUM_ANALYZER_HPP

void displaySpectrumAnalyzer(uint8_t brightness_p, bool rainbow, bool normalizeBands, uint8_t sensitivity_p, uint8_t speed_p);
void setupSpectrumAnalyzer();
void collectSamples();

#endif
