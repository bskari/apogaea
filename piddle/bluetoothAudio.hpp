#ifndef BLUETOOTH_AUDIO_HPP
#define BLUETOOTH_AUDIO_HPP

#include "constants.hpp"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

void setupBluetoothAudio(TaskHandle_t collectSamplesTask, const char* deviceName);
void teardownBluetoothAudio();

#endif // BLUETOOTH_AUDIO_HPP
