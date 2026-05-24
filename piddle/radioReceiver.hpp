#pragma once
#include "radio.hpp"

void setupRadioReceiver();
bool pollRadioReceiver(RadioConfigMessage_t& out);
