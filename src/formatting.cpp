#include "formatting.h"

// Convierte float a String con control de decimales. En Arduino suele ser mas
// portable que confiar en formatos printf con floats.
String formatFloat(float value, uint8_t decimals) {
    char buffer[20];
    dtostrf(value, 0, decimals, buffer);
    return String(buffer);
}
