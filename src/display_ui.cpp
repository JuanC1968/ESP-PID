#include "display_ui.h"

#include <Wire.h>

#include "app_globals.h"
#include "formatting.h"

namespace {

// Traduce un porcentaje 0..100 a una coordenada Y de la grafica. En pantalla, Y
// crece hacia abajo, por eso se invierte el calculo.
uint8_t graphY(float percent) {
    percent = constrain(percent, 0.0f, 100.0f);
    return GRAPH_Y + GRAPH_HEIGHT - 1 - static_cast<uint8_t>((percent * (GRAPH_HEIGHT - 1)) / 100.0f);
}

}

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
