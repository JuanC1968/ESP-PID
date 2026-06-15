#pragma once

#include <Arduino.h>

void writeActuator(float output);
void resetPidState();
void updatePid(uint32_t nowMs);
