#include "sensor.h"

#include "app_globals.h"

// Lee el ADC varias veces y devuelve la luz como porcentaje 0..100. El promedio
// reduce ruido instantaneo del ADC y del divisor LDR.
float readLightPercent(uint16_t &raw) {
    uint32_t accumulator = 0;
    for (uint8_t i = 0; i < ADC_SAMPLES; i++) {
        accumulator += analogRead(PIN_LDR);
        delayMicroseconds(250);
    }

    raw = accumulator / ADC_SAMPLES;
    float percent = (static_cast<float>(raw) * 100.0f) / 4095.0f;
    if (config.invertSensor) {
        percent = 100.0f - percent;
    }
    return constrain(percent, 0.0f, 100.0f);
}

// Filtro exponencial: combina una parte pequeña de la lectura nueva con la lectura
// filtrada anterior. Baja SENSOR_FILTER_ALPHA para mas suavidad, subelo para mas rapidez.
float readFilteredLightPercent() {
    state.rawPercent = readLightPercent(state.raw);
    if (!state.filterReady) {
        state.input = state.rawPercent;
        state.filterReady = true;
    } else {
        state.input += SENSOR_FILTER_ALPHA * (state.rawPercent - state.input);
    }

    return state.input;
}
