# Parametric Grid for LED Matrix

This is for generating grid support for Neopixel matrices (ws8212b LED panels). The support can be 3d printed and used to hold a diffuser in front of the LED panel. I added slots on the face touching the LED matrix to leave room for the tiny SMD capacitors. If for some reason you have an LED matrix that does not expose any SMD components, you can set the "SlotsForSmd" parameter to false.

To test this model, I used Cura 5.5.0 slicer to import the included [Grid 16x16 with SMD Slots.stl](<./Grid 16x16 with SMD Slots.stl>) STL file. In Cura, I got the exact results I wanted by setting Cura's print settings to use a 0.6 mm nozzle and I set "Top/Bottom Thickness" to "1.2 mm" and "Wall Ordering" to "Outside to Inside". I also used a 0.6 mm nozzle on my Ender 3 V2, printed with PLA.

Using the above settings, the outside wall of the grid has only 1 wall/line, and the grid squares within have 2 walls/lines. This allows you to use multiple grids, side by side, achieving the same wall width from end to end for each LED/pixel.

See below for a rendering of the 16x16 configuration in OpenSCAD:

![Parametric Grid for LED Matrix](<../images/OpenScad - Parametric Grid for LED Matrix.png>)
