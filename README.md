# Sand Matrix

This project uses an MCU and 1 or more WS2812x LED panels to display colorful sand falling and piling, changing color over time. The "new sand" color changes slowly over time, creating a gradient of fallen sand colors. The fallen sand also changes color over time, so the gradient dynamically updates.

I have tested and ran this on a Wemos D1 Mini ESP32 and an ESP32 S3 DevkitC. Both work just as well. The [platformio.ini](platformio.ini) file has configuration sections for both of these boards.

The display uses the [FastLED](https://github.com/FastLED/FastLED) library to output to 1 or more WS2812x LED panels.
 
For the LED panels, there are 2 configurations supported.

- A single 16 x 16 LED matrix
  - Output to the LED strip is on a single pin.
- Nine 16 x 16 LED matrices, squared
  - Output to this configuration is on three pins. The LED panel configuration is made up of a set of three (logical) 16x48 vertical panels. This set is made up of three sets of three 16x16 panels connected vertically, and then the set arranged horizontally.

To choose the configuration you want, you can set one of the following in the [main.cpp](src/main.cpp) file:
```
#define ARRAY_SIZE_16x16
// #define ARRAY_SIZE_48x48
```

The default is the single 16 x 16 panel.
