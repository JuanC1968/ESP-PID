# Esquema KiCad

Este directorio contiene un esquema inicial para KiCad:

- `ESP-PID.pro`
- `ESP-PID.sch`

El esquema esta en formato legacy de KiCad para que sea texto sencillo y versionable. Al abrirlo con una version moderna de KiCad, guardalo una vez y KiCad lo convertira al formato actual `.kicad_pro` / `.kicad_sch`.

## Suposiciones del esquema

- Placa ESP32 compatible con `denky32`.
- OLED SH1107 por I2C a 3V3.
- Divisor LDR: `3V3 -- LDR -- GPIO34 -- 10k -- GND`.
- Condensador opcional entre `GPIO34` y `GND`, sugerido `100 nF` a `1 uF`.
- LED blanco directo desde GPIO25 con resistencia serie. Si el LED consume demasiado, conviene cambiarlo por transistor/MOSFET.
- LEDs verde y rojo con transistores NPN en conmutacion low-side.

## Pines

| Funcion | GPIO |
| --- | ---: |
| LDR analogica | 34 |
| LED blanco PWM | 25 |
| LED verde SET | 26 |
| LED rojo NO SET | 27 |
| I2C SDA | 21 |
| I2C SCL | 22 |
