#pragma once

#include <Arduino.h>

// Pines del montaje. GPIO34 es solo entrada, ideal para leer el divisor del LDR.
constexpr uint8_t PIN_LDR = 34;
constexpr uint8_t PIN_LED_WHITE = 25;
constexpr uint8_t PIN_LED_GREEN = 26;
constexpr uint8_t PIN_LED_RED = 27;
constexpr uint8_t PIN_I2C_SDA = 21;
constexpr uint8_t PIN_I2C_SCL = 22;

// PWM del LED blanco: 10 bits dan valores de salida entre 0 y 1023.
constexpr uint8_t PWM_CHANNEL = 0;
constexpr uint16_t PWM_FREQUENCY = 5000;
constexpr uint8_t PWM_RESOLUTION = 10;
constexpr uint16_t PWM_MAX = (1 << PWM_RESOLUTION) - 1;

// Tiempos de trabajo y parametros de suavizado/estado.
constexpr uint32_t CONTROL_INTERVAL_MS = 100;
constexpr uint32_t DISPLAY_INTERVAL_MS = 250;
constexpr uint32_t SERIAL_INTERVAL_MS = 1000;
constexpr uint8_t ADC_SAMPLES = 16;
constexpr float SENSOR_FILTER_ALPHA = 0.12f;

// Histeresis del LED verde: entra en SET por debajo de SET_ENTER_BAND y no sale
// hasta superar SET_EXIT_BAND. Si ambos son iguales, se comporta como tolerancia simple.
constexpr float SET_ENTER_BAND = 1.0f;
constexpr float SET_EXIT_BAND = 1.0f;

// Zona de la mini grafica en la OLED.
constexpr uint8_t GRAPH_WIDTH = 116;
constexpr uint8_t GRAPH_X = 6;
constexpr uint8_t GRAPH_Y = 74;
constexpr uint8_t GRAPH_HEIGHT = 48;

constexpr const char *AP_SSID = "ESP-PID";
constexpr const char *AP_PASSWORD = "12345678";

// Parametros que el usuario puede modificar desde la pagina web y que se guardan
// en la memoria no volatil del ESP32.
struct PidConfig {
    float setpoint = 55.0f;
    float kp = 7.0f;
    float ki = 0.6f;
    float kd = 0.15f;
    float outMin = 0.0f;
    float outMax = static_cast<float>(PWM_MAX);
    bool invertSensor = false;
};

// Valores calculados en tiempo real. Separar config y estado ayuda a ver que se
// guarda en memoria y que solo existe mientras el programa esta corriendo.
struct RuntimeState {
    float input = 0.0f;
    float rawPercent = 0.0f;
    float output = 0.0f;
    float error = 0.0f;
    float integral = 0.0f;
    float derivative = 0.0f;
    float lastError = 0.0f;
    uint16_t raw = 0;
    bool inSet = false;
    bool filterReady = false;
};
