#include <Arduino.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <U8g2lib.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>

#include "app_config.h"
#include "app_globals.h"
#include "control.h"
#include "display_ui.h"
#include "storage.h"
#include "telemetry.h"
#include "web_server.h"

// Constructor especifico para OLED SH1107 128x128 I2C. La variante SEEED corrige
// el offset de columnas que hacia que la imagen se envolviera por los lados.
U8G2_SH1107_SEEED_128X128_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);

PidConfig config;
RuntimeState state;
Preferences preferences;
WebServer server(80);
bool fileSystemReady = false;

uint32_t lastControlMs = 0;
uint32_t lastDisplayMs = 0;
uint32_t lastSerialMs = 0;

// Buffer circular para la grafica. Cuando llega al final, vuelve al principio y
// va sobrescribiendo las muestras mas antiguas.
float graphInput[GRAPH_WIDTH] = {};
float graphSetpoint[GRAPH_WIDTH] = {};
uint8_t graphIndex = 0;
uint8_t graphCount = 0;

void setup() {
    Serial.begin(115200);
    delay(200);

    // LEDs de estado. Asumimos transistores NPN: HIGH enciende.
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_RED, OUTPUT);
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_RED, HIGH);

    analogReadResolution(12);
    analogSetPinAttenuation(PIN_LDR, ADC_11db);

    // Configura el PWM del LED que ilumina la LDR.
    ledcSetup(PWM_CHANNEL, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcAttachPin(PIN_LED_WHITE, PWM_CHANNEL);
    writeActuator(0);

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(100000);
    uint8_t oledAddress = scanI2cBus();
    if (oledAddress == 0x3C || oledAddress == 0x3D) {
        display.setI2CAddress(oledAddress << 1);
    }
    display.begin();
    display.setContrast(180);
    display.setPowerSave(0);
    display.clearBuffer();
    display.setFont(u8g2_font_6x12_tf);
    display.drawStr(8, 14, "OLED OK");
    display.drawStr(8, 30, "ESP-PID");
    display.sendBuffer();

    // Punto de acceso propio: no necesita router para configurar el controlador.
    loadConfig();
    fileSystemReady = LittleFS.begin(true);
    if (!fileSystemReady) {
        Serial.println(F("LittleFS no disponible. Sube la carpeta data con uploadfs."));
    }
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    startWebServer();

    Serial.println();
    Serial.println(F("ESP PID LDR iniciado"));
    Serial.print(F("Conecta a WiFi: "));
    Serial.println(AP_SSID);
    Serial.print(F("Clave: "));
    Serial.println(AP_PASSWORD);
    Serial.print(F("Web: http://"));
    Serial.println(WiFi.softAPIP());
}

void loop() {
    uint32_t nowMs = millis();

    // El loop reparte tiempo entre web, control, pantalla y serie. No usa delay()
    // para que la pagina responda mientras el PID sigue trabajando.
    server.handleClient();
    updatePid(nowMs);

    if (nowMs - lastDisplayMs >= DISPLAY_INTERVAL_MS) {
        lastDisplayMs = nowMs;
        drawDisplay();
    }

    if (nowMs - lastSerialMs >= SERIAL_INTERVAL_MS) {
        lastSerialMs = nowMs;
        printStatus();
    }
}
