#ifndef BLUETOOTH_AUDIO_HPP
#define BLUETOOTH_AUDIO_HPP

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Start the A2DP Bluetooth sink with the given device name.
// collectSamplesTask is suspended when a source connects and resumed on disconnect.
void setupBluetoothAudio(TaskHandle_t collectSamplesTask, const char* deviceName);

#endif
