# Wiring Diagram

All sensors are connected to the ESP8266 through one shared I2C bus.

## Power

```text
WeMos 3.3V -> breadboard + rail
WeMos GND  -> breadboard - rail
```

All sensor modules are powered from 3.3 V.

## ESP8266 I2C

```text
GPIO4 -> SDA
GPIO5 -> SCL
```

## BH1750

```text
BH1750 VCC  -> 3.3V
BH1750 GND  -> GND
BH1750 SDA  -> GPIO4
BH1750 SCL  -> GPIO5
BH1750 ADDR -> not connected
```

Detected I2C address: `0x23`.

## AHT20 + BMP280 Combined Module

Current module orientation on the breadboard:

```text
row 33 -> VDD
row 32 -> SDA
row 31 -> GND
row 30 -> SCL
```

Connections:

```text
VDD -> 3.3V
GND -> GND
SDA -> GPIO4
SCL -> GPIO5
```

Detected I2C addresses:

- AHT20: `0x38`
- BMP280: `0x77`

## Current Breadboard Wiring

### WeMos to breadboard

```text
3.3V -> + power rail
GND  -> - power rail
SCL / GPIO5 -> breadboard g12
SDA / GPIO4 -> breadboard g13
```

### BH1750

```text
VCC  -> g10 -> 3.3V
GND  -> g11 -> GND
SCL  -> g12
SDA  -> g13
ADDR -> g14 position, not electrically connected
```

### AHT20 + BMP280

```text
VDD -> row 33 -> 3.3V line
SDA -> row 32 -> SDA line
GND -> row 31 -> GND line
SCL -> row 30 -> SCL line
```

The second module is connected in parallel with the BH1750 on the same SDA and SCL lines.

## I2C Bus Structure

```text
                    +--> BH1750 (0x23)
                    |
ESP8266 SDA GPIO4 --+--> AHT20  (0x38)
                    |
                    +--> BMP280 (0x77)

                    +--> BH1750
                    |
ESP8266 SCL GPIO5 --+--> AHT20
                    |
                    +--> BMP280
```

All three I2C devices share the same SDA and SCL lines and are distinguished by their unique I2C addresses.
