#include "control.h"

#include "app_globals.h"
#include "sensor.h"

namespace {

// Anade la muestra actual a la grafica usando el buffer circular.
void addGraphSample() {
    graphInput[graphIndex] = state.input;
    graphSetpoint[graphIndex] = config.setpoint;
    graphIndex = (graphIndex + 1) % GRAPH_WIDTH;
    if (graphCount < GRAPH_WIDTH) {
        graphCount++;
    }
}

// Control de los LEDs de estado con banda de histeresis para evitar parpadeos
// cuando la variable esta justo en el borde del setpoint.
void updateStatusLeds() {
    float absError = fabsf(state.error);
    if (state.inSet) {
        state.inSet = absError <= SET_EXIT_BAND;
    } else {
        state.inSet = absError <= SET_ENTER_BAND;
    }

    digitalWrite(PIN_LED_GREEN, state.inSet ? HIGH : LOW);
    digitalWrite(PIN_LED_RED, state.inSet ? LOW : HIGH);
}

}

// Aplica la salida PID al PWM del LED blanco.
void writeActuator(float output) {
    uint16_t duty = static_cast<uint16_t>(constrain(output, 0.0f, static_cast<float>(PWM_MAX)));
    ledcWrite(PWM_CHANNEL, duty);
}

// Reinicia las partes internas del PID. Se usa al cambiar parametros para que la
// integral vieja no arrastre el nuevo ajuste.
void resetPidState() {
    state.integral = 0.0f;
    state.derivative = 0.0f;
    state.lastError = 0.0f;
    state.filterReady = false;
}

// Ejecuta el controlador cada CONTROL_INTERVAL_MS. Es no bloqueante: si aun no ha
// pasado el tiempo, sale y deja al loop atender la web y la pantalla.
void updatePid(uint32_t nowMs) {
    if (lastControlMs == 0) {
        lastControlMs = nowMs;
        state.input = readFilteredLightPercent();
        return;
    }

    uint32_t elapsedMs = nowMs - lastControlMs;
    if (elapsedMs < CONTROL_INTERVAL_MS) {
        return;
    }

    float dt = static_cast<float>(elapsedMs) / 1000.0f;
    lastControlMs = nowMs;

    state.input = readFilteredLightPercent();
    state.error = config.setpoint - state.input;
    state.integral += state.error * dt;
    state.derivative = (state.error - state.lastError) / dt;

    // PID discreto: P responde al error actual, I al error acumulado y D a la
    // velocidad de cambio del error.
    float unclamped = (config.kp * state.error) + (config.ki * state.integral) + (config.kd * state.derivative);
    state.output = constrain(unclamped, config.outMin, config.outMax);

    // Anti-windup simple: si la salida esta saturada y la integral empuja aun mas
    // hacia la saturacion, deshacemos la ultima acumulacion integral.
    bool saturatedHigh = unclamped > config.outMax && state.error > 0.0f;
    bool saturatedLow = unclamped < config.outMin && state.error < 0.0f;
    if (saturatedHigh || saturatedLow) {
        state.integral -= state.error * dt;
    }

    state.lastError = state.error;
    writeActuator(state.output);
    updateStatusLeds();
    addGraphSample();
}
