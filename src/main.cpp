#include <Arduino.h>
#include <Preferences.h>
#include <U8g2lib.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>

// Constructor especifico para OLED SH1107 128x128 I2C. La variante SEEED corrige
// el offset de columnas que hacia que la imagen se envolviera por los lados.
U8G2_SH1107_SEEED_128X128_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);

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

const char *AP_SSID = "ESP-PID";
const char *AP_PASSWORD = "12345678";

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

PidConfig config;
RuntimeState state;
Preferences preferences;
WebServer server(80);

uint32_t lastControlMs = 0;
uint32_t lastDisplayMs = 0;
uint32_t lastSerialMs = 0;

// Buffer circular para la grafica. Cuando llega al final, vuelve al principio y
// va sobrescribiendo las muestras mas antiguas.
float graphInput[GRAPH_WIDTH] = {};
float graphSetpoint[GRAPH_WIDTH] = {};
uint8_t graphIndex = 0;
uint8_t graphCount = 0;

// Escaner de bus para diagnosticar la pantalla OLED. Si no aparece 0x3C o 0x3D,
// normalmente hay un problema de cableado/alimentacion o SDA/SCL intercambiados.
uint8_t scanI2cBus() {
    uint8_t foundAddress = 0;

    Serial.println(F("Escaneando bus I2C..."));
    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();
        if (error == 0) {
            Serial.print(F("I2C encontrado en 0x"));
            if (address < 16) {
                Serial.print('0');
            }
            Serial.println(address, HEX);
            if (foundAddress == 0) {
                foundAddress = address;
            }
        }
    }

    if (foundAddress == 0) {
        Serial.println(F("No hay dispositivos I2C. Revisa VCC, GND, SDA y SCL."));
    }

    return foundAddress;
}

// Evita que valores dinamicos puedan romper el HTML si contienen caracteres especiales.
String htmlEscape(const String &text) {
    String escaped;
    escaped.reserve(text.length());
    for (char c : text) {
        switch (c) {
        case '&': escaped += F("&amp;"); break;
        case '<': escaped += F("&lt;"); break;
        case '>': escaped += F("&gt;"); break;
        case '"': escaped += F("&quot;"); break;
        default: escaped += c; break;
        }
    }
    return escaped;
}

// Convierte float a String con control de decimales. En Arduino suele ser mas
// portable que confiar en formatos printf con floats.
String formatFloat(float value, uint8_t decimals = 2) {
    char buffer[20];
    dtostrf(value, 0, decimals, buffer);
    return String(buffer);
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

// Traduce un porcentaje 0..100 a una coordenada Y de la grafica. En pantalla, Y
// crece hacia abajo, por eso se invierte el calculo.
uint8_t graphY(float percent) {
    percent = constrain(percent, 0.0f, 100.0f);
    return GRAPH_Y + GRAPH_HEIGHT - 1 - static_cast<uint8_t>((percent * (GRAPH_HEIGHT - 1)) / 100.0f);
}

// Anade la muestra actual a la grafica usando el buffer circular.
void addGraphSample() {
    graphInput[graphIndex] = state.input;
    graphSetpoint[graphIndex] = config.setpoint;
    graphIndex = (graphIndex + 1) % GRAPH_WIDTH;
    if (graphCount < GRAPH_WIDTH) {
        graphCount++;
    }
}

// Aplica la salida PID al PWM del LED blanco.
void writeActuator(float output) {
    uint16_t duty = static_cast<uint16_t>(constrain(output, 0.0f, static_cast<float>(PWM_MAX)));
    ledcWrite(PWM_CHANNEL, duty);
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

// Dibuja la pantalla OLED: datos compactos arriba y grafica de SP/PV abajo.
void drawDisplay() {
    constexpr uint8_t x = 2;

    display.clearBuffer();
    display.setFont(u8g2_font_6x12_tf);
    display.drawStr(x, 10, "PID LDR");
    display.setCursor(70, 10);
    display.print(state.inSet ? F("SET") : F("NO SET"));

    display.setCursor(x, 25);
    display.print(F("SP "));
    display.print(formatFloat(config.setpoint, 1));
    display.print(F("%"));

    display.setCursor(66, 25);
    display.print(F("PV "));
    display.print(formatFloat(state.input, 1));
    display.print(F("%"));

    display.setCursor(x, 40);
    display.print(F("E "));
    display.print(formatFloat(state.error, 1));

    display.setCursor(66, 40);
    display.print(F("PWM "));
    display.print(formatFloat((state.output * 100.0f) / PWM_MAX, 0));
    display.print(F("%"));

    display.setCursor(x, 55);
    display.print(F("Kp "));
    display.print(formatFloat(config.kp, 1));
    display.print(F(" Ki "));
    display.print(formatFloat(config.ki, 1));

    display.setCursor(x, 68);
    display.print(F("Kd "));
    display.print(formatFloat(config.kd, 2));
    display.print(F(" IP 4.1"));

    display.drawHLine(GRAPH_X, graphY(50.0f), GRAPH_WIDTH);
    display.drawPixel(GRAPH_X - 2, graphY(100.0f));
    display.drawPixel(GRAPH_X - 2, graphY(0.0f));

    // PV se dibuja como linea continua. SP se dibuja con puntos para distinguirlo
    // sin necesitar colores en una pantalla monocroma.
    int16_t previousInputX = -1;
    int16_t previousInputY = -1;
    uint8_t firstX = GRAPH_X + GRAPH_WIDTH - graphCount;
    for (uint8_t i = 0; i < graphCount; i++) {
        uint8_t sampleIndex = (graphCount == GRAPH_WIDTH) ? (graphIndex + i) % GRAPH_WIDTH : i;
        uint8_t px = firstX + i;
        uint8_t inputY = graphY(graphInput[sampleIndex]);
        uint8_t setY = graphY(graphSetpoint[sampleIndex]);

        if ((i % 3) == 0) {
            display.drawPixel(px, setY);
        }
        if (previousInputX >= 0) {
            display.drawLine(previousInputX, previousInputY, px, inputY);
        }
        previousInputX = px;
        previousInputY = inputY;
    }

    display.sendBuffer();
}

// Construye la pagina HTML en memoria. La pagina no se recarga sola: usa JavaScript
// para actualizar solo los numeros, y pausa esa actualizacion mientras editas.
String renderPage() {
    String checked = config.invertSensor ? F(" checked") : F("");
    String page;
    page.reserve(5200);
    page += F("<!doctype html><html lang='es'><head><meta charset='utf-8'>");
    page += F("<meta name='viewport' content='width=device-width,initial-scale=1'>");
    page += F("<title>ESP PID LDR</title><style>");
    page += F(":root{font-family:system-ui,-apple-system,Segoe UI,sans-serif;color:#17202a;background:#eef3f1}");
    page += F("body{margin:0;padding:24px}main{max-width:760px;margin:auto}section{background:#fff;border:1px solid #ccd7d2;border-radius:8px;padding:18px;margin:0 0 16px}");
    page += F("h1{font-size:1.6rem;margin:0 0 14px}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(145px,1fr));gap:12px}");
    page += F(".metric{border-left:4px solid #277c68;padding:8px 10px;background:#f6faf8}.metric b{display:block;font-size:1.45rem}");
    page += F("label{display:block;font-weight:650;margin:12px 0 5px}input{width:100%;box-sizing:border-box;font:inherit;padding:9px;border:1px solid #aebbb6;border-radius:6px}");
    page += F(".check{display:flex;gap:8px;align-items:center;margin-top:14px}.check input{width:auto}button{font:inherit;font-weight:700;border:0;border-radius:6px;padding:11px 16px;background:#1f6f5d;color:white;cursor:pointer}");
    page += F(".row{display:grid;grid-template-columns:1fr 1fr;gap:12px}.status{font-weight:800;color:");
    page += state.inSet ? F("#137a36") : F("#a93226");
    page += F("}@media(max-width:560px){body{padding:12px}.row{grid-template-columns:1fr}}</style>");
    page += F("</head><body><main>");
    page += F("<section><h1>Controlador PID</h1><div class='grid'>");
    page += F("<div class='metric'>Luz<b><span id='input'>");
    page += htmlEscape(formatFloat(state.input, 1));
    page += F("</span>%</b></div><div class='metric'>Setpoint<b><span id='setpointValue'>");
    page += htmlEscape(formatFloat(config.setpoint, 1));
    page += F("</span>%</b></div><div class='metric'>Error<b><span id='error'>");
    page += htmlEscape(formatFloat(state.error, 1));
    page += F("</span></b></div><div class='metric'>Salida PWM<b><span id='output'>");
    page += htmlEscape(formatFloat((state.output * 100.0f) / PWM_MAX, 1));
    page += F("</span>%</b></div><div class='metric'>Estado<b id='state' class='status'>");
    page += state.inSet ? F("SET") : F("NO SET");
    page += F("</b></div><div class='metric'>ADC raw<b><span id='raw'>");
    page += String(state.raw);
    page += F("</span></b></div><div class='metric'>Luz sin filtro<b><span id='rawPercent'>");
    page += htmlEscape(formatFloat(state.rawPercent, 1));
    page += F("</span>%</b></div></div></section>");

    page += F("<section><form method='post' action='/config'><div class='row'><div>");
    page += F("<label for='setpoint'>Setpoint luz (%)</label><input id='setpoint' name='setpoint' type='number' min='0' max='100' step='0.1' value='");
    page += htmlEscape(formatFloat(config.setpoint, 1));
    page += F("'><label for='kp'>Kp</label><input id='kp' name='kp' type='number' step='0.01' value='");
    page += htmlEscape(formatFloat(config.kp, 2));
    page += F("'><label for='ki'>Ki</label><input id='ki' name='ki' type='number' step='0.01' value='");
    page += htmlEscape(formatFloat(config.ki, 2));
    page += F("'><label for='kd'>Kd</label><input id='kd' name='kd' type='number' step='0.01' value='");
    page += htmlEscape(formatFloat(config.kd, 2));
    page += F("'></div><div><label for='outMin'>PWM minimo</label><input id='outMin' name='outMin' type='number' min='0' max='1023' step='1' value='");
    page += htmlEscape(formatFloat(config.outMin, 0));
    page += F("'><label for='outMax'>PWM maximo</label><input id='outMax' name='outMax' type='number' min='0' max='1023' step='1' value='");
    page += htmlEscape(formatFloat(config.outMax, 0));
    page += F("'><label class='check'><input name='invert' type='checkbox'");
    page += checked;
    page += F(">Invertir lectura del sensor</label></div></div><p><button type='submit'>Guardar parametros</button></p></form>");
    page += F("<form method='post' action='/reset'><button type='submit'>Reiniciar PID</button></form>");
    page += F("<p id='refreshState' style='color:#52635c;font-size:.92rem'>Actualizacion automatica activa</p></section>");
    page += F("<script>");
    page += F("const f=document.querySelector(\"form[action='/config']\"),s=document.getElementById('refreshState');let editing=false;");
    page += F("function txt(id,v){const e=document.getElementById(id);if(e)e.textContent=v}");
    page += F("function setEditing(v){editing=v;s.textContent=v?'Actualizacion pausada mientras editas':'Actualizacion automatica activa'}");
    page += F("f.addEventListener('focusin',()=>setEditing(true));f.addEventListener('focusout',()=>setTimeout(()=>setEditing(f.contains(document.activeElement)),80));");
    page += F("async function updateStatus(){if(editing)return;try{const r=await fetch('/status',{cache:'no-store'});const d=await r.json();");
    page += F("txt('input',Number(d.input).toFixed(1));txt('setpointValue',Number(d.setpoint).toFixed(1));txt('error',Number(d.error).toFixed(1));");
    page += F("txt('output',(Number(d.output)*100/1023).toFixed(1));txt('raw',d.raw);txt('rawPercent',Number(d.rawPercent).toFixed(1));");
    page += F("const st=document.getElementById('state');st.textContent=d.inSet?'SET':'NO SET';st.style.color=d.inSet?'#137a36':'#a93226';}catch(e){}}");
    page += F("setInterval(updateStatus,1000);</script>");
    page += F("</main></body></html>");
    return page;
}

// Rutas del servidor web.
void handleRoot() {
    server.send(200, "text/html", renderPage());
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

    server.sendHeader("Location", "/", true);
    server.send(303, "text/plain", "");
}

// Boton web para reiniciar solo el estado interno del PID.
void handleReset() {
    resetPidState();
    server.sendHeader("Location", "/", true);
    server.send(303, "text/plain", "");
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

// Registra las URL disponibles.
void startWebServer() {
    server.on("/", HTTP_GET, handleRoot);
    server.on("/config", HTTP_POST, handleConfig);
    server.on("/reset", HTTP_POST, handleReset);
    server.on("/status", HTTP_GET, handleStatusJson);
    server.begin();
}

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
