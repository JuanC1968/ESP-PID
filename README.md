# ESP-PID

Controlador PID simulado con un ESP32 WROOM: un LED blanco ilumina una LDR y el firmware regula el PWM para alcanzar el setpoint de luz configurado.

## Hardware previsto

- ESP32 WROOM / placa compatible `denky32`
- OLED 128x128 por I2C
- LDR en entrada analogica
- LED blanco controlado por PWM
- LED verde para estado `SET`, por transistor
- LED rojo para estado `NO SET`, por transistor

## Pines por defecto

| Funcion | GPIO |
| --- | ---: |
| LDR analogica | 34 |
| LED blanco PWM | 25 |
| LED verde SET | 26 |
| LED rojo NO SET | 27 |
| I2C SDA | 21 |
| I2C SCL | 22 |

Los pines estan definidos al principio de `src/main.cpp`.

## LDR y estabilidad

Conexion recomendada del divisor:

```text
3V3 --- LDR --- GPIO34 --- 10k --- GND
```

Para amortiguar oscilaciones electricas, coloca un condensador entre `GPIO34` y `GND`, lo mas cerca posible del ESP32:

- `100 nF`: filtrado ligero, buena primera prueba.
- `1 uF`: filtrado mas suave, suele ir bien con LDR.
- `4.7 uF` o `10 uF`: mucha amortiguacion, la lectura responde mas lenta.

El firmware tambien filtra la lectura:

- Promedia 16 muestras ADC por ciclo.
- Aplica un filtro exponencial con `SENSOR_FILTER_ALPHA = 0.12`.
- El LED verde entra en `SET` con error menor o igual que `2.5%`.
- Sale de `SET` cuando el error supera `4.0%`.

Estos valores estan definidos al principio de `src/main.cpp`.

## OLED

El codigo usa este constructor de U8g2:

```cpp
U8G2_SH1107_SEEED_128X128_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);
```

La pantalla muestra arriba los valores `SP`, `PV`, error, PWM, estado, ganancias PID e IP compacta. La zona inferior es una mini grafica sin marco:

- Linea continua: variable medida `PV`.
- Linea punteada: setpoint `SP`.
- Linea horizontal central: referencia del 50%.

## Web

Al arrancar crea un punto de acceso WiFi:

- SSID: `ESP-PID`
- Clave: `12345678`
- URL: `http://192.168.4.1`

Desde la web se pueden cambiar:

- Setpoint de luz
- `Kp`, `Ki`, `Kd`
- PWM minimo y maximo
- Inversion de lectura del sensor

Los parametros se guardan en memoria no volatil con `Preferences`.

## Compilar y cargar

```bash
pio run
pio run --target upload
pio device monitor
```
