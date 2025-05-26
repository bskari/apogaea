#include <Arduino.h>
#include <assert.h>
#include <driver/i2s_std.h>
#include <esp_check.h>
#include <esp_log.h>
#include <FastLED.h>
#include <stdint.h>


#include "I2SClocklessLedDriver/I2SClocklessLedDriver.h"
#include "constants.hpp"
#include "esp32-fft.hpp"

// Set this to 1 if you want to from both ends of the LED strips. Primarily used for testing and
// development when I don't want to have 10 strips all hooked up at once.
#ifndef DOUBLE_ENDED
#  define DOUBLE_ENDED 0
#endif

static const int I2S_SAMPLE_RATE_HZ = 44100; // Sample rate of the I2S microphone
static const int MAX_I2S_BUFFER_LENGTH = 512;

static const int MINIMUM_THRESHOLD = 20;

// Generated from python3 steps.py 2048 44100
static const int SAMPLE_COUNT = 2048;
static constexpr uint16_t NOTE_TO_OUTPUT_INDEX[] = {
  1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 16, 18, 20, 22, 24, 27, 30, 32, 36, 40, 45, 48, 54, 61, 64, 72, 81, 91, 97, 109, 122, 129, 145, 163, 183
};
const int c4Index = 11;

static const int NOTE_COUNT = COUNT_OF(NOTE_TO_OUTPUT_INDEX);
static_assert(NOTE_TO_OUTPUT_INDEX[NOTE_COUNT - 1] < SAMPLE_COUNT / 2, "Too few samples to represent all notes");

// Slide down twice to make it move faster (just 1 for developing)
const int SLIDE_COUNT = 1;

static float input[SAMPLE_COUNT];
static float output[SAMPLE_COUNT];

// This probably doesn't need to be souble the SAMPLE_COUNT, SAMPLE_COUNT + MAX_I2S_BUFFER_LENGTH
// (or maybe + MAX_I2S_BUFFER_LENGTH * 2) would probably cut it. Ah well, this works. If I run low
// on memory, it's something to consider.
static int16_t rawSamples[SAMPLE_COUNT * 2];
static volatile int rawSamplesOffset = 0;
fft_config_t* realFftPlan = nullptr;

static float weightingConstants[SAMPLE_COUNT];
static float windowingConstants[SAMPLE_COUNT];

static i2s_chan_handle_t rxHandle;

extern CRGB leds[STRIP_COUNT][LEDS_PER_STRIP];
extern I2SClocklessLedDriver driver;
extern bool logDebug;

static void computeFft();
static void renderFft(bool rainbow, bool normalizeBands);
static void slideDown(int count);
static float maxOutputForNote(int note);
static void normalizeTo0_1(float samples[], int length, float minimumDivisor);
static void logOutputNotes();
static void logNotes(const float noteValues[NOTE_COUNT]);
static float aWeightingMultiplier(const float frequency);
static float windowingMultiplier(const int offset);
static void powerOfTwo(float* const array, const int length);
static constexpr float square(const float f);

void RemoteXY_delayFunction(int ms);


static void computeFft() {
  // Windowing
  for (int i = 0; i < COUNT_OF(input); ++i) {
    input[i] *= windowingConstants[i];
  }

  // Call this directly instead of through fft_execute for dead code elimination
  rfft(input, output, realFftPlan->twiddle_factors, COUNT_OF(input));

  // These entries represent the average power or something? Just clear them
  output[0] = 0.0;
  output[1] = 0.0;
  output[SAMPLE_COUNT - 1] = 0.0;

  powerOfTwo(output, COUNT_OF(output));

#if false
  // Debug logging
  const int interval_ms = 5000;
  static decltype(millis()) nextDisplayTime_ms = interval_ms;
  if (millis() < nextDisplayTime_ms) {
    return;
  }
  nextDisplayTime_ms = millis() + interval_ms;
  for (int i = 0; i < COUNT_OF(output) / 2; ++i) {
    Serial.printf("%03d:%f\n", i, output[i]);
  }
#endif
}

static void renderFft(const bool rainbow, const bool normalizeBands) {
  const int startNote = c4Index - 4;

  // Okay. So there are 5 strands that I'm going to loop down and back up. I want the bassline to be
  // on the outside edge, going up, and the other notes to trickle down from the center.

  // Slide down more than once to make it move faster (just 1 for developing)
  slideDown(SLIDE_COUNT);

  float noteValues[NOTE_COUNT];
  for (int note = 0; note < NOTE_COUNT; ++note) {
    noteValues[note] = maxOutputForNote(note);
  }
  if (logDebug) {
    logOutputNotes();
    logNotes(noteValues);
    const auto unscaledMw = calculate_unscaled_power_mW(reinterpret_cast<CRGB*>(leds), LEDS_PER_STRIP * STRIP_COUNT);
    Serial.printf("%ld mW (%ldmA@5V,%ldmA@12V) if at max brightness\n", unscaledMw, unscaledMw / 5, unscaledMw / 12);
    logDebug = false;
  }

  int strip = 0;
  for (int i = 0; i < STRIP_COUNT; ++i) {
    for (int j = 0; j < SLIDE_COUNT; ++j) {
      leds[i][j] = CRGB::Black;
      #if DOUBLE_ENDED
        leds[i][LEDS_PER_STRIP - j - 1] = CRGB::Black;
      #endif
    }
  }

  static uint8_t rainbowOffset = 0;
  constexpr uint16_t hue16Step = 256 * 3;
  // Vary the start hue by a small sine wave
  const int quadWaveMillisDiv = 64;
  const int quadWaveDiv = 8;

  const uint8_t hueStart = [rainbow]() mutable {
    if (rainbow) {
      ++rainbowOffset;
      return rainbowOffset;
    }
    return static_cast<uint8_t>(quadwave8(millis() / quadWaveMillisDiv) / quadWaveDiv - 20);
  }();

  // I'm having each color represent a different range. Red low, green mid, blue
  // high. So try normalizing each band? Or at least boost them a bit.
  if (normalizeBands) {
    float allMaxValue = noteValues[note];
    for (int i = 0; i < STRIP_COUNT * 3; ++i) {
      allMaxValue = max(allMaxValue, noteValues[note]);
    }
    for (int range = 0; range < 3; ++range) {
      const int start = startNote + STRIP_COUNT * range;
      const int end = start + STRIP_COUNT;
      float maxValue = noteValues[start];
      for (int note = start + 1; note < end; ++note) {
        maxValue = max(maxValue, noteValues[note]);
      }
      // Average the allMaxValue and maxValue, that way if it's really quiet in
      // one band, we're just boosting somewhat
      const float inverse = 1.0f / ((maxValue + allMaxValue) * 0.5f);
      for (int note = start; note < end; ++note) {
        noteValues[note] *= inverse;
      }
    }
  }

  uint16_t hue16 = hueStart * 256;
  for (int note = startNote; note < COUNT_OF(noteValues) - 1; /* Increment done in loop */) {
    {
      const float floatValue = noteValues[note];
      // Multiply by 254 instead of 255 so I don't need to worry about wraparound. Should be 0 <=
      // floatValue <= 1, but just in case.
      const uint8_t intValue = static_cast<uint8_t>(floatValue * 254);
      // Fast and fairly accurate gamma correction
      const uint8_t gammaCorrected = intValue * intValue / 255;
      const uint8_t hue = (hue16 >> 8);
      hue16 += hue16Step;
      // Do SLIDE_COUNT + 1 because the first LED is the logic level shifter on the PCB
      for (int i = 1; i < SLIDE_COUNT + 1; ++i) {
        leds[strip][i] += CHSV(hue, 255, gammaCorrected);
      }
    }
    ++note;

    #if DOUBLE_ENDED
    {
      const float floatValue = noteValues[note];
      // Multiply by 254 instead of 255 so I don't need to worry about wraparound. Should be 0 <=
      // floatValue <= 1, but just in case.
      const uint8_t intValue = static_cast<uint8_t>(floatValue * 254);
      // Fast and fairly accurate gamma correction
      const uint8_t gammaCorrected = intValue * intValue / 255;
      const uint8_t hue = (hue16 >> 8);
      hue16 += hue16Step;
      for (int i = 0; i < SLIDE_COUNT; ++i) {
        leds[strip][LEDS_PER_STRIP - 1 - i] += CHSV(hue, 255, gammaCorrected);
      }
    }
    ++note;
    #endif

    ++strip;
    if (strip >= STRIP_COUNT) {
      strip = 0;
      constexpr uint16_t step = 65536 / 3 - hue16Step * STRIP_COUNT * 2;
      static_assert(step > 0);
      hue16 += step;
    }
  }

  // Shut off the ends of the strips
  for (int i = 0; i < STRIP_COUNT; ++i) {
    for (int j = LEDS_PER_STRIP - 35; j < LEDS_PER_STRIP; ++j) {
      leds[i][j] = CRGB::Black;
    }
  }
}

void collectSamples() {
  // This algorithm won't work if either of these are false
  static_assert(SAMPLE_COUNT > MAX_I2S_BUFFER_LENGTH);
  static_assert(SAMPLE_COUNT % MAX_I2S_BUFFER_LENGTH == 0);

  const int usPerSample = 1000 * 1000 / I2S_SAMPLE_RATE_HZ;
  // We're just going to continually read into rawSamples
  size_t bytesRead;
  auto start_us = micros();
  auto diff_us = micros();
  while (1) {
    const int oldOffset = rawSamplesOffset;
    // Mark this section as unable to be used
    rawSamplesOffset += MAX_I2S_BUFFER_LENGTH;
    if (rawSamplesOffset >= COUNT_OF(rawSamples)) {
      rawSamplesOffset = 0;
    }

    diff_us = micros() - start_us;
    if (diff_us < usPerSample) {
      delayMicroseconds(usPerSample - diff_us);
    }

    // TODO: Could use i2s_channel_register_event_callback() and change the delay to 0 to make this
    // async. Then use this core to do more processing
    ESP_ERROR_CHECK(
      i2s_channel_read(
        rxHandle,
        &rawSamples[oldOffset],
        MAX_I2S_BUFFER_LENGTH * sizeof(rawSamples[0]),
        &bytesRead,
        portMAX_DELAY
      )
    );
    assert(bytesRead == MAX_I2S_BUFFER_LENGTH * sizeof(rawSamples[0]));
  }
}

void displaySpectrumAnalyzer(
  uint8_t brightness_p,
  const bool rainbow,
  const bool normalizeBands,
  const uint8_t sensitivity_p,
  const uint8_t speed_p
) {
  const decltype(millis()) logTime_ms = 5000;
  static auto next_ms = 1000;
  static int loopCount = 0;
  #if SHOW_VOLTAGE
    static int voltageOnes = 4, voltageTenths = 7, voltageHundredths = 1;
  #endif

  auto part_us = micros();
  // First we need to copy the data from the samples circular buffer
  // We can't use memcpy because we're converting uint16_t to float
  // Also, these are supposed to be coming in as int16_t, but looks like they're coming in as unsigned?
  // Seeing samples like... 1 2 3 3 2 1 0 32767 32766 32765
  // TODO: Might be able to fix this with the i2s options, like bit_shift or msb_right. Try those.
  // Screw it, just correct it. I tried to do this in the sample thread, but there's not enough time
  // to do it in between i2s_channel_reads. Maybe if I was calling that async?
  #define FIX_SAMPLE_SIGN(value) ((value) < 0x4000 ? (value) : -(0x8000 - 1 - (value)))

  // Copy up to the most recent data
  auto sampleOffset = rawSamplesOffset - SAMPLE_COUNT;
  if (sampleOffset < 0) {
    sampleOffset += COUNT_OF(rawSamples);
  }

  if (sampleOffset + COUNT_OF(input) < COUNT_OF(rawSamples)) {
    for (int i = 0; i < COUNT_OF(input); ++i) {
      input[i] = FIX_SAMPLE_SIGN(rawSamples[sampleOffset + i]);
    }
  } else {
    // Let's say length = 10, offset = 13
    // Then I need to copy 7 items (2*length-offset) starting at 13
    const int upper = COUNT_OF(rawSamples) - sampleOffset;
    for (int i = 0; i < upper; ++i) {
      input[i] = FIX_SAMPLE_SIGN(rawSamples[sampleOffset + i]);
    }
    // Then copy the last 3 items
    const int lower = COUNT_OF(output) - upper;
    for (int i = 0; i < lower; ++i) {
      input[i + upper] = FIX_SAMPLE_SIGN(rawSamples[i]);
    }
  }

  if (logDebug) {
    Serial.println("Samples");
    // I don't need all 2048 outputs, just get 50
    for (int i = 0; i < 50; ++i) {
      Serial.printf("%d ", FIX_SAMPLE_SIGN(rawSamples[(i + sampleOffset) % COUNT_OF(rawSamples)]));
    }
    Serial.println();
  }
  const auto samples_us = micros() - part_us;

  part_us = micros();
  computeFft();
  // Bass lines have more energy than higher samples, so reduce them
  for (int i = 0; i < COUNT_OF(output); ++i) {
    output[i] *= weightingConstants[i];
  }
  // Minimum divisor. The output from the FFT is squared, and we could sqrt it, but that's slow and
  // unnecessary, so just square this number. Lower this number to increase sensitivity.
  // 50% = 4000
  const float minimumDivisor = square((130 - sensitivity_p) * 50);
  normalizeTo0_1(output, SAMPLE_COUNT, minimumDivisor);
  const auto compute_us = micros() - part_us;

  part_us = micros();
  renderFft(rainbow, normalizeBands);
  const auto render_us = micros() - part_us;

  const int sliderBrightness = static_cast<float>(brightness_p) * 255.0f / 100.0f;
  // My voltage converter can only output 10A, so limit to half of that
  const int max_ma = 5000;
  const int limitedBrightness = calculate_max_brightness_for_power_vmA(
    reinterpret_cast<CRGB*>(leds),
    STRIP_COUNT * LEDS_PER_STRIP,
    sliderBrightness,
    12,
    max_ma
  );
  driver.setBrightness(limitedBrightness);

  part_us = micros();
  driver.showPixels(NO_WAIT);
  const auto show_us = micros() - part_us;

  // The animations are too fast, so add an artificial delay
  const int delay_ms = 100 - speed_p;
  RemoteXY_delayFunction(delay_ms);

  ++loopCount;
  if (millis() > next_ms) {
    Serial.printf("%f FPS with %dms delay\n", static_cast<double>(loopCount) * 1000 / logTime_ms, delay_ms);
    Serial.printf(
      "samples_us:%lu compute_us:%lu render_us:%lu show_us:%lu\n",
      samples_us,
      compute_us,
      render_us,
      show_us
    );

    #if SHOW_VOLTAGE
      const float R1 = 10000.0f;
      const float R2 = 5100.0f;
      const float referenceVoltage = 3.3f;
      const float maxReading = 4095;
      int adcReading = analogRead(VOLTAGE_PIN);
      const float adcVoltage = static_cast<float>(adcReading) / maxReading * referenceVoltage;
      const float voltage = adcVoltage * (R1 + R2) / R2;
      Serial.printf(
        "Voltage:%0.2f (adc:%d adcV:%0.2f)\n",
        voltage,
        adcReading,
        adcVoltage);
      char buffer[6];
      snprintf(buffer, 6, "%0.2f", voltage);
      voltageOnes = buffer[0] - '0';
      voltageTenths = buffer[2] - '0';
      voltageHundredths = buffer[3] - '0';
    #endif

    next_ms = millis() + logTime_ms;
    loopCount = 0;
  }

}

static void slideDown(const int count) {
  #if DOUBLE_ENDED
    int byteCount = (LEDS_PER_STRIP / 2 - count) * sizeof(leds[0][0]);
    if (LEDS_PER_STRIP % 2 == 1) {
      byteCount += sizeof(leds[0][0]);
    }
  #else
    const int byteCount = (LEDS_PER_STRIP - count) * sizeof(leds[0][0]);
  #endif

  for (int i = 0; i < STRIP_COUNT; ++i) {
    memmove(
      &leds[i][count],
      &leds[i][0],
      byteCount
    );
    #if DOUBLE_ENDED
      memmove(
        &leds[i][LEDS_PER_STRIP / 2],
        &leds[i][LEDS_PER_STRIP / 2 + count],
        byteCount
      );
    #endif
  }
}

void setupSpectrumAnalyzer() {
  i2s_chan_config_t channelConfig = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
  // The above macro sets channelConfig to {
  //  .id = I2S_NUM_AUTO,
  //  .role = I2S_ROLE_MASTER,
  //  .dma_desc_num = 6,
  //  .auto_clear_after_cb = 0,
  //  .auto_clear_before_cb = 0,
  //  .intr_priority = 0,
  // }

  // Send nullptr for the tx handle, we're only receiving
  ESP_ERROR_CHECK(i2s_new_channel(&channelConfig, nullptr, &rxHandle));

  i2s_std_slot_config_t slotConfig = {
    .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
    .slot_bit_width = I2S_SLOT_BIT_WIDTH_16BIT,
    .slot_mode = I2S_SLOT_MODE_MONO,
    .slot_mask = I2S_STD_SLOT_LEFT, // TODO
    .ws_pol = false,
    .bit_shift = false,
    .msb_right = false,
  };

  i2s_std_config_t stdConfig = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE_HZ),
    .slot_cfg = slotConfig,
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = GPIO_NUM_5,
      .ws = GPIO_NUM_18,
      .dout = I2S_GPIO_UNUSED,
      .din = GPIO_NUM_19,
      .invert_flags = {
        .mclk_inv = false,
        .bclk_inv = false,
        .ws_inv = false,
      },
    },
  };
  ESP_ERROR_CHECK(i2s_channel_init_std_mode(rxHandle, &stdConfig));

  ESP_ERROR_CHECK(i2s_channel_enable(rxHandle));

  realFftPlan = fft_init(SAMPLE_COUNT, FFT_REAL, FFT_FORWARD, input, output);

  for (int i = 0; i < COUNT_OF(windowingConstants); ++i) {
    windowingConstants[i] = windowingMultiplier(i);
  }

  // Bass notes have higher percieved energy, because the human ear is weird. To compensate, we'll
  // do A weighting.
  for (int i = 0; i < COUNT_OF(weightingConstants); ++i) {
    const float freq = static_cast<float>(I2S_SAMPLE_RATE_HZ) / SAMPLE_COUNT * (i + 1);
    weightingConstants[i] = aWeightingMultiplier(freq);
  }

  std::fill(rawSamples, rawSamples + COUNT_OF(rawSamples), 0);
}

constexpr float square(const float f) {
  return f * f;
}

/**
 * Returns the A weighting multiplier for a frequency, see https://en.wikipedia.org/wiki/A-weighting
 */
static float aWeightingMultiplier(const float frequency) {
  const float freq_2 = square(frequency);
  const float denom1 = freq_2 + square(20.6f);
  const float denom2 = sqrtf((freq_2 + square(107.7f)) * (freq_2 + square(737.9f)));
  const float denom3 = freq_2 + square(12194.0f);
  const float denom = denom1 * denom2 * denom3;
  const float enumer = (square(freq_2) * square(12194.0f));
  const float ra = enumer / denom;
  const float aWeighting_db = 2.0f + 20.0f * logf(ra) / logf(10.0f);
  const float multiplier = powf(10.0f, aWeighting_db / 10.0f);
  return multiplier;
}

/**
 * Returns the windowing multiplier, see https://en.wikipedia.org/wiki/Window_function
 */
static float windowingMultiplier(const int offset) {
  // a0 = 0.5 for Hann, a0 = 0.54 for original Hamming, a0 = 0.53836 for new Hamming
  const float a0 = 0.53836;
  return a0 - (1.0f - a0) * cosf(TWO_PI * static_cast<float>(offset) / COUNT_OF(input));
}

/**
 * Returns the max output value in a particular note. For example, if NOTE_TO_OUTPUT_INDEX[c4Index] =
 * 39 and NOTE_TO_OUTPUT_INDEX[c4Index] = 43, it will look in output[39:43] and return the largest value.
 */
static float maxOutputForNote(const int note) {
  if (note >= COUNT_OF(NOTE_TO_OUTPUT_INDEX) - 1) {
    return output[COUNT_OF(NOTE_TO_OUTPUT_INDEX) - 1];
  }
  float maxOutput = output[NOTE_TO_OUTPUT_INDEX[note]];
  for (int i = NOTE_TO_OUTPUT_INDEX[note]; i < NOTE_TO_OUTPUT_INDEX[note + 1]; ++i) {
    maxOutput = max(maxOutput, output[i]);
  }
  return maxOutput;
}


/**
 * Normalize the samples to [0..1], or lower if all the samples are low
 */
static void normalizeTo0_1(float samples[], const int length, const float minimumDivisor) {
  float minSample = samples[0];
  for (int i = 1; i < length; ++i) {
    minSample = min(minSample, samples[i]);
  }
  for (int i = 0; i < length; ++i) {
    samples[i] -= minSample;
  }
  float maxSample = samples[0];
  for (int i = 1; i < length; ++i) {
    maxSample = max(maxSample, samples[i]);
  }

  // Always have some divisor, in case all the values are low
  const float divisor = max(maxSample, minimumDivisor);

  // Map them all to 0.0 .. 1.0
  const float multiplier = 1.0 / divisor;
  for (int i = 0; i < length; ++i) {
    samples[i] = samples[i] * multiplier;
  }
}

void powerOfTwo(float* const array, const int length) {
  for (int i = 0; i < length / 2; ++i) {
    array[i] = array[i * 2] * array[i * 2] + array[i * 2 + 1] * array[i * 2 + 1];
  }
}

static void logOutputNotes() {
  Serial.println("Output at notes:");
  for (int i = 0; i < 20; ++i) {
    Serial.printf("%d:%0.2f ", NOTE_TO_OUTPUT_INDEX[i], output[NOTE_TO_OUTPUT_INDEX[i]]);
  }
  Serial.println();
}

static void logNotes(const float noteValues[NOTE_COUNT]) {
  Serial.println("Notes:");

  char note = 'C';
  int digit = 4;

  // Find start note
  for (int i = 0; i < c4Index; ++i) {
    if (note == 'C') {
      --digit;
      --note;
    } else if (note == 'A') {
      note = 'G';
    } else {
      --note;
    }
  }

  for (int i = 0; i < NOTE_COUNT; ++i) {
    Serial.printf("%c%d:%d ", note, digit, static_cast<int>(noteValues[i] * 255));
    if (note == 'B') {
      ++digit;
      ++note;
    } else if (note == 'G') {
      note = 'A';
    } else {
      ++note;
    }
  }
  Serial.println();
}
