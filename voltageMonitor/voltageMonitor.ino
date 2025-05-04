#include <cstdint>
//////////////////////////////////////////////
//        RemoteXY include library          //
//////////////////////////////////////////////

// you can enable debug logging to Serial at 115200
//#define REMOTEXY__DEBUGLOG

#include "BLEDevice.h"
#include <RemoteXY.h>

// RemoteXY GUI configuration
#pragma pack(push, 1)
uint8_t RemoteXY_CONF[] =   // 196 bytes
  { 255,2,0,34,0,189,0,19,0,0,0,118,111,108,116,97,103,101,45,109,
  111,110,105,116,111,114,0,24,1,106,200,1,1,15,0,67,59,15,40,10,
  78,2,26,3,129,8,14,43,12,64,17,86,111,108,116,97,103,101,0,129,
  15,41,29,12,64,17,79,102,102,32,86,0,67,59,41,40,10,78,2,26,
  3,4,9,67,89,13,128,2,26,67,37,144,40,10,69,2,26,10,129,15,
  55,30,12,64,17,79,102,102,32,37,0,67,59,54,40,10,78,2,26,1,
  129,25,27,10,12,64,17,37,0,67,59,28,40,10,78,2,26,1,67,61,
  82,38,10,78,2,26,3,67,61,96,38,10,78,2,26,1,129,3,82,56,
  12,64,17,82,101,115,117,109,101,32,86,0,129,3,95,58,12,64,17,82,
  101,115,117,109,101,32,37,0,4,9,108,89,13,128,2,26 };

// this structure defines all the variables and events of your control interface
struct {

    // input variables
  int8_t offVoltageSlider; // from 0 to 100
  int8_t resumeVoltageSlider; // from 0 to 100

    // output variables
  float voltage;
  float offVoltage;
  char status[10]; // string UTF8 end zero
  float offPercent;
  float voltagePercent;
  float resumeVoltage;
  float resumePercent;

    // other variable
  uint8_t connect_flag;  // =1 if wire connected, else =0

} RemoteXY;
#pragma pack(pop)

/////////////////////////////////////////////
//           END RemoteXY include          //
/////////////////////////////////////////////

#include <Arduino.h>

#define COUNT_OF(x) (sizeof((x)) / sizeof((0[x])))

#ifndef LED_BUILTIN
  #define LED_BUILTIN 2
#endif

static const int VOLTAGE_PIN = 4;
static const int RELAY_PIN = 5;

CRemoteXY* remotexy;

// According to https://shopsolarkits.com/blogs/learning-center/marine-battery-voltage-chart,
// 12.2V is 50% capacity, 12.0V is 25%, 11.98 is 20%, 11.90 is 0%
const float minimumOff_v = 11.98f;

// Originally I was going to have 3 states: Normal, Off, and Charging, but I
// think can just do these 2
enum class State {
  On,
  Off,
};

static bool show = false;

static float adcSlopeCorrection = 0.0f;
static float adcInterceptCorrection = 0.0f;

static float adcReadingToVoltage(float rawAdc);
static float voltageToUndividedVoltage(float voltage);
static float correctedVoltage(float voltage);
static constexpr float voltageToPercent(float voltage);
static constexpr float sliderToVoltage(int slider);
static void updateRemoteXY(const State state, const float battery_v);
static int test();

constexpr float sliderToVoltage(const int slider) {
  // Map to 12.0V-13.1V, or 10%-60%
  return static_cast<float>(slider) / 100.f * (13.1f - 12.0f) + 12.0f;
}

constexpr float voltageToPercent(const float voltage) {
  // https://www.renogy.com/blog/lifepo4-voltage-chart
  // Note that this chart (and others) list both 40% and 50% as 13.0V
  if (voltage >= 13.4f) {
    return (voltage - 13.4f) / (13.6f - 13.4f) * (100.0f - 90.0f) + 90.0f;
  }
  if (voltage >= 13.0f) {
    return (voltage - 13.0f) / (13.4f - 13.0f) * (90.0f - 50.0f) + 50.0f;
  }
  if (voltage >= 12.8f) {
    return (voltage - 12.8f) / (13.0f - 12.8f) * (40.0f - 20.0f) + 20.0f;
  }
  if (voltage >= 12.0f) {
    return (voltage - 12.0f) / (12.8f - 12.0f) * (20.0f - 10.0f) + 10.0f;
  }
  return (voltage - 10.0f) / (12.0f - 10.0f) * (10.0f - 0.0f) + 0.0f;
}


void setup() {
  Serial.begin(115200);
  const int failCount = test();
  if (failCount > 0) {
    while (1) {
      test();
      delay(5000);
    }
  }

  remotexy = new CRemoteXY(
    RemoteXY_CONF_PROGMEM,
    &RemoteXY,
    new CRemoteXYStream_BLEDevice("voltage")
  );

  pinMode(RELAY_PIN, OUTPUT);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  pinMode(VOLTAGE_PIN, INPUT);

  // Calculate a line of best fit from ADC reports and ones from my multimeter
  constexpr float multimeterReadings_v[] = {13.21};
  constexpr float adcReadings_v[] = {14.67};
  static_assert(COUNT_OF(multimeterReadings_v) == COUNT_OF(adcReadings_v));
  if (COUNT_OF(multimeterReadings_v) == 0) {
    adcSlopeCorrection = 1.0f;
    adcInterceptCorrection = 0.0f;
  } else if (COUNT_OF(multimeterReadings_v) == 1) {
    adcSlopeCorrection = 1.0f;
    adcInterceptCorrection = multimeterReadings_v[0] - adcReadings_v[0];
  } else {
    float sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0;
    const int n = COUNT_OF(multimeterReadings_v);
    for (int i = 0; i < n; i++) {
      sum_x += adcReadings_v[i];
      sum_y += multimeterReadings_v[i];
      sum_xy += adcReadings_v[i] * multimeterReadings_v[i];
      sum_x2 += adcReadings_v[i] * adcReadings_v[i];
    }
    adcSlopeCorrection = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);
    adcInterceptCorrection = (sum_y - adcSlopeCorrection * sum_x) / n;
    Serial.printf("true V = %f * adc V + %f\n", adcSlopeCorrection, adcInterceptCorrection);
    for (int i = 0; i < COUNT_OF(multimeterReadings_v); ++i) {
      const float corrected = correctedVoltage(adcReadings_v[i]);
      Serial.printf(
        "true V:%0.2f adc V:%0.2f corrected:%0.2f diff:%0.4f\n",
        multimeterReadings_v[i],
        adcReadings_v[i],
        corrected,
        multimeterReadings_v[i] - corrected
      );
    }
  }
  if (adcSlopeCorrection != adcSlopeCorrection) {
    adcSlopeCorrection = 1.0f;
    Serial.println("adcSlopeCorrection is nan, resetting");
  }
  if (adcInterceptCorrection != adcInterceptCorrection) {
    adcInterceptCorrection = 0.0f;
    Serial.println("adcInterceptCorrection is nan, resetting");
  }

  // If the battery is good, then turn on the LEDs immediately
  const int adcReading = analogRead(VOLTAGE_PIN);
  const float adc_v = adcReadingToVoltage(adcReading);
  const float battery_v = voltageToUndividedVoltage(adc_v);
  const float correctedBattery_v = correctedVoltage(battery_v);
  if (correctedBattery_v > RemoteXY.offVoltage) {
    // When wiring, hook the strips to the relay normally open, so that we need to take action to close it
    digitalWrite(RELAY_PIN, HIGH);
  } else {
    digitalWrite(RELAY_PIN, LOW);
  }

  setCpuFrequencyMhz(80);

  //RemoteXY_Init();

  constexpr float defaultOffVoltage = 12.4f;
  constexpr int defaultOffVoltageSlider = 45;
  RemoteXY.offVoltageSlider = defaultOffVoltageSlider;
  constexpr float offVoltage = sliderToVoltage(defaultOffVoltageSlider);
  RemoteXY.offVoltage = offVoltage;
  // Slider value should match
  static_assert(defaultOffVoltage - 0.1f <= offVoltage);
  static_assert(offVoltage <= defaultOffVoltage + 0.1f);
  constexpr float offPercent = voltageToPercent(offVoltage);
  RemoteXY.offPercent = offPercent;
  // Should be 16%
  constexpr float expectedCutoffPercent = 16.0f;
  static_assert(expectedCutoffPercent - 1.0f <= offPercent);
  static_assert(offPercent <= expectedCutoffPercent + 1.0f);

  constexpr float defaultResumeVoltage = 13.0f;  // ~50%
  constexpr int defaultResumeVoltageSlider = 85;
  RemoteXY.resumeVoltageSlider = defaultResumeVoltageSlider;
  constexpr float resumeVoltage = sliderToVoltage(defaultResumeVoltageSlider);
  RemoteXY.resumeVoltage = resumeVoltage;
  // Slider value should match
  static_assert(defaultResumeVoltage - 0.1f <= resumeVoltage);
  static_assert(resumeVoltage <= defaultResumeVoltage + 0.1f);
  constexpr float resumePercent = voltageToPercent(resumeVoltage);
  RemoteXY.resumePercent = resumePercent;
  // Should be 33%
  constexpr float expectedResumePercent = 33.0f;
  static_assert(expectedResumePercent - 1.0f <= resumePercent);
  static_assert(resumePercent <= expectedResumePercent + 1.0f);

  Serial.println("setup done");
}

void loop() {
  const int adcReadingCount = 10;

  // We don't want to keep switching the relay, and because there's voltage drop
  // under load, just shut it off if it drops below offThreshold_v and keep it
  // off until it goes back above warningThreshold_v
  const decltype(millis()) relayToggleDelay_ms = 3 * 60 * 1000;
  auto relayToggleTime_ms = millis();

  State state;

  {
    const int adcReading = analogRead(VOLTAGE_PIN);
    const float adc_v = adcReadingToVoltage(adcReading);
    const float battery_v = voltageToUndividedVoltage(adc_v);
    const float correctedBattery_v = correctedVoltage(battery_v);
    state = (correctedBattery_v > RemoteXY.offVoltage) ? State::On : State::Off;
  }

  decltype(millis()) next_ms = 0;

  while (1) {
    // Average a few readings
    float adcReading = 0.0f;
    for (int i = 0; i < adcReadingCount; ++i) {
      adcReading += analogRead(VOLTAGE_PIN);
    }
    adcReading /= adcReadingCount;
    const float adc_v = adcReadingToVoltage(adcReading);
    const float battery_v = voltageToUndividedVoltage(adc_v);
    const float correctedBattery_v = correctedVoltage(battery_v);

    if (millis() > next_ms || show) {
      Serial.printf(
        "Corrected batV:%0.2f (adc:%0.2f adcV:%0.2f, batV:%0.2f)\n",
        correctedBattery_v,
        adcReading,
        adc_v,
        battery_v);

      while (Serial.available()) {
        const auto line = Serial.readStringUntil('\n');
        Serial.println("Arduino says:");
        Serial.println(line);
      }
      next_ms = millis() + 1000;
    }

    if (show) {
      char buffer[6];
      snprintf(buffer, 6, "%05.2f", correctedBattery_v);
      const int voltageTens = buffer[0] - '0';
      const int voltageOnes = buffer[1] - '0';
      const int voltageTenths = buffer[3] - '0';
      const int voltageHundredths = buffer[4] - '0';

      blinkNumber(voltageTens);
      blinkNumber(voltageOnes);
      blinkNumber(voltageTenths);
      blinkNumber(voltageHundredths);

      show = false;
    }

    // Only update the relay occasionally, to avoid thrashing
    if (millis() > relayToggleTime_ms + relayToggleDelay_ms) {
      relayToggleTime_ms = millis();

      switch (state) {
        case State::On:
          if (correctedBattery_v < RemoteXY.offVoltage) {
            Serial.printf("Turning off strips, V=%0.2f<%0.2f\n", correctedBattery_v, RemoteXY.offVoltage);
            state = State::Off;
            digitalWrite(RELAY_PIN, LOW);
          }
          break;
        case State::Off:
          if (correctedBattery_v > RemoteXY.resumeVoltage) {
            Serial.printf("Turning on strips, V=%0.2f>%0.2f\n", correctedBattery_v, RemoteXY.resumeVoltage);
            state = State::On;
            digitalWrite(RELAY_PIN, HIGH);
          }
          break;
      }
    }

    remotexy->handler();
    updateRemoteXY(state, correctedBattery_v);
  }
}


void updateRemoteXY(const State state, const float battery_v) {
  switch (state) {
    case State::On:
      strcpy(RemoteXY.status, "On");
      break;
    case State::Off:
      static_assert(strlen("Off") < COUNT_OF(RemoteXY.status));
      strcpy(RemoteXY.status, "Off");
      break;
  }
  RemoteXY.voltage = battery_v;
  RemoteXY.voltagePercent = voltageToPercent(battery_v);

  // We could only recompute these sliders if they change, but who cares
  if (RemoteXY.offVoltageSlider == 100) {
    RemoteXY.offVoltageSlider = 99;
  }
  RemoteXY.offVoltage = sliderToVoltage(RemoteXY.offVoltageSlider);
  RemoteXY.offPercent = voltageToPercent(RemoteXY.offVoltage);
  if (RemoteXY.offVoltage < minimumOff_v) {
    RemoteXY.offVoltage = minimumOff_v;
    RemoteXY.offVoltageSlider = 12;
  }

  if (RemoteXY.resumeVoltageSlider <= RemoteXY.offVoltageSlider) {
    RemoteXY.resumeVoltageSlider = RemoteXY.offVoltageSlider + 1;
  }
  RemoteXY.resumeVoltage = sliderToVoltage(RemoteXY.resumeVoltageSlider);
  RemoteXY.resumePercent = voltageToPercent(RemoteXY.resumeVoltage);
}

void blinkNumber(const int count) {
  if (count < 0 || count > 9) {
    Serial.printf("Tried to blink %d\n", count);
    for (int i = 0; i < 10; ++i) {
      digitalWrite(LED_BUILTIN, HIGH);
      remotexy->delay(50);
      digitalWrite(LED_BUILTIN, LOW);
      remotexy->delay(50);
    }
  } else if (count == 0) {
    digitalWrite(LED_BUILTIN, HIGH);
    remotexy->delay(500);
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    for (int i = 0; i < count; ++i) {
      digitalWrite(LED_BUILTIN, HIGH);
      remotexy->delay(200);
      digitalWrite(LED_BUILTIN, LOW);
      remotexy->delay(200);
    }
  }
  remotexy->delay(500);
}

float adcReadingToVoltage(const float adcReading) {
  const float maxReading = 4095.0f;
  const float referenceVoltage = 3.3f;
  return adcReading / maxReading * referenceVoltage;
}

float voltageToUndividedVoltage(const float voltage) {
  // If you made your own voltage divider circuit, use this
  // const float R1 = 20000.0f;
  // const float R2 = 5100.0f;
  // return voltage * (R1 + R2) / R2;

  // If you're using a commercial module, just use the divider value
  return voltage * 5.0f;
}

float correctedVoltage(const float voltage) {
  return voltage * adcSlopeCorrection + adcInterceptCorrection;
}

#define EXPECT(assertion) { \
  ++test; \
  if ((assertion)) { \
    /* Serial.printf("%d passed\n", test); */ \
  } else { \
    Serial.printf("%d failed: %s\n", test, #assertion); \
    ++fails; \
  } \
}
#define ALMOST_EQUAL(a, b) { \
  ++test; \
  if ((a) - 0.001 < (b) && (a) + 0.001 > (b)) { \
    /* Serial.printf("%d passed\n", test); */ \
  } else { \
    Serial.printf("%d failed: %s != %s\n", test, #a, #b); \
    ++fails; \
  } \
}
int test() {
  int test = 0;
  int fails = 0;
  ALMOST_EQUAL(voltageToPercent(13.6f), 100.0f);
  ALMOST_EQUAL(voltageToPercent(13.4f), 90.0f);
  EXPECT(voltageToPercent(13.5f) > 90.0f);
  EXPECT(voltageToPercent(13.5f) < 100.0f);
  ALMOST_EQUAL(voltageToPercent(13.3f), 80.0f);
  ALMOST_EQUAL(voltageToPercent(13.2f), 70.0f);
  ALMOST_EQUAL(voltageToPercent(12.9f), 30.0f);
  ALMOST_EQUAL(voltageToPercent(12.0f), 10.0f);
  return fails;
}
