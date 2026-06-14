#pragma once

#include <Preferences.h>
#include <U8g2lib.h>
#include <WebServer.h>

#include "app_config.h"

extern U8G2_SH1107_SEEED_128X128_F_HW_I2C display;
extern PidConfig config;
extern RuntimeState state;
extern Preferences preferences;
extern WebServer server;
extern bool fileSystemReady;

extern uint32_t lastControlMs;
extern uint32_t lastDisplayMs;
extern uint32_t lastSerialMs;

extern float graphInput[GRAPH_WIDTH];
extern float graphSetpoint[GRAPH_WIDTH];
extern uint8_t graphIndex;
extern uint8_t graphCount;
