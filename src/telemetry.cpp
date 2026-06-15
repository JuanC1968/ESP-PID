#include "telemetry.h"

#include <Arduino.h>

#include "app_globals.h"

// Telemetria por monitor serie para ajustar y depurar sin depender de la web.
void printStatus() {
    Serial.print(F("Luz="));
    Serial.print(state.input, 1);
    Serial.print(F("% Set="));
    Serial.print(config.setpoint, 1);
    Serial.print(F("% Error="));
    Serial.print(state.error, 1);
    Serial.print(F(" PWM="));
    Serial.print(state.output, 0);
    Serial.print(F(" Estado="));
    Serial.println(state.inSet ? F("SET") : F("NO SET"));
}
