#include "web_server.h"

#include <LittleFS.h>

#include "app_globals.h"
#include "control.h"
#include "formatting.h"
#include "storage.h"

namespace {

bool streamFile(const char *path, const char *contentType) {
    if (!fileSystemReady) {
        server.send(503, "text/plain", "LittleFS no esta disponible");
        return false;
    }

    File file = LittleFS.open(path, "r");
    if (!file) {
        server.send(404, "text/plain", "Archivo no encontrado");
        return false;
    }

    server.streamFile(file, contentType);
    file.close();
    return true;
}

// Lee un campo numerico enviado por formulario. Si no existe, conserva el valor anterior.
float readParameter(const String &name, float fallback) {
    if (!server.hasArg(name)) {
        return fallback;
    }
    return server.arg(name).toFloat();
}

// Los checkboxes HTML solo se envian cuando estan marcados.
bool readCheckbox(const String &name) {
    return server.hasArg(name) && server.arg(name) == "on";
}

// Rutas del servidor web.
void handleRoot() {
    streamFile("/index.html", "text/html");
}

void handleStyle() {
    streamFile("/style.css", "text/css");
}

void handleScript() {
    streamFile("/app.js", "application/javascript");
}

// Recibe el formulario, valida rangos basicos, reinicia el PID y guarda en NVS.
void handleConfig() {
    config.setpoint = constrain(readParameter("setpoint", config.setpoint), 0.0f, 100.0f);
    config.kp = readParameter("kp", config.kp);
    config.ki = readParameter("ki", config.ki);
    config.kd = readParameter("kd", config.kd);
    config.outMin = constrain(readParameter("outMin", config.outMin), 0.0f, static_cast<float>(PWM_MAX));
    config.outMax = constrain(readParameter("outMax", config.outMax), 0.0f, static_cast<float>(PWM_MAX));
    if (config.outMin > config.outMax) {
        float previousMin = config.outMin;
        config.outMin = config.outMax;
        config.outMax = previousMin;
    }
    config.invertSensor = readCheckbox("invert");
    resetPidState();
    saveConfig();

    server.send(200, "text/plain", "OK");
}

// Boton web para reiniciar solo el estado interno del PID.
void handleReset() {
    resetPidState();
    server.send(200, "text/plain", "OK");
}

// Configuracion guardada para rellenar el formulario de la pagina estatica.
void handleConfigJson() {
    String json;
    json.reserve(180);
    json += F("{\"setpoint\":");
    json += formatFloat(config.setpoint, 2);
    json += F(",\"kp\":");
    json += formatFloat(config.kp, 2);
    json += F(",\"ki\":");
    json += formatFloat(config.ki, 2);
    json += F(",\"kd\":");
    json += formatFloat(config.kd, 2);
    json += F(",\"outMin\":");
    json += formatFloat(config.outMin, 0);
    json += F(",\"outMax\":");
    json += formatFloat(config.outMax, 0);
    json += F(",\"invert\":");
    json += config.invertSensor ? F("true") : F("false");
    json += F("}");
    server.send(200, "application/json", json);
}

// Datos vivos en JSON para que la pagina actualice las tarjetas sin recargar.
void handleStatusJson() {
    String json;
    json.reserve(320);
    json += F("{\"input\":");
    json += formatFloat(state.input, 2);
    json += F(",\"setpoint\":");
    json += formatFloat(config.setpoint, 2);
    json += F(",\"error\":");
    json += formatFloat(state.error, 2);
    json += F(",\"output\":");
    json += formatFloat(state.output, 0);
    json += F(",\"raw\":");
    json += String(state.raw);
    json += F(",\"rawPercent\":");
    json += formatFloat(state.rawPercent, 2);
    json += F(",\"inSet\":");
    json += state.inSet ? F("true") : F("false");
    json += F("}");
    server.send(200, "application/json", json);
}

}

// Registra las URL disponibles.
void startWebServer() {
    server.on("/", HTTP_GET, handleRoot);
    server.on("/index.html", HTTP_GET, handleRoot);
    server.on("/style.css", HTTP_GET, handleStyle);
    server.on("/app.js", HTTP_GET, handleScript);
    server.on("/config", HTTP_POST, handleConfig);
    server.on("/config.json", HTTP_GET, handleConfigJson);
    server.on("/reset", HTTP_POST, handleReset);
    server.on("/status", HTTP_GET, handleStatusJson);
    server.begin();
}
