#include "storage.h"

#include "app_globals.h"

// Guarda la configuracion PID en NVS para recuperarla despues de reiniciar.
void saveConfig() {
    preferences.begin("pid", false);
    preferences.putFloat("setpoint", config.setpoint);
    preferences.putFloat("kp", config.kp);
    preferences.putFloat("ki", config.ki);
    preferences.putFloat("kd", config.kd);
    preferences.putFloat("outMin", config.outMin);
    preferences.putFloat("outMax", config.outMax);
    preferences.putBool("invert", config.invertSensor);
    preferences.end();
}

// Carga la configuracion guardada. Si no existe todavia, conserva los valores por defecto.
void loadConfig() {
    preferences.begin("pid", true);
    config.setpoint = preferences.getFloat("setpoint", config.setpoint);
    config.kp = preferences.getFloat("kp", config.kp);
    config.ki = preferences.getFloat("ki", config.ki);
    config.kd = preferences.getFloat("kd", config.kd);
    config.outMin = preferences.getFloat("outMin", config.outMin);
    config.outMax = preferences.getFloat("outMax", config.outMax);
    config.invertSensor = preferences.getBool("invert", config.invertSensor);
    preferences.end();
}
