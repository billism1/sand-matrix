// Parametric LED Matrix Grid
//
// This is for generating grid support for Neopixel matrices (ws8212/ws8212b LED panels).
//
// Designed for printing a grid to be placed on the front of LED panels/matrices, so that a diffuser can be placed on top of that.
// This design leaves a gap for the SMD capacitors mounted under the LEDs.
//
// Optionally, the "IncludeDiffuser" flag can be set to true so that you can print 1 (or 2) layers of white (or similar color) filament,
// pause the print, and then replace the filament with black or another darker color. Or you could just print the whole thing in white
// if you don't mind the light colors blending together a little bit.
//
// I set the "WallWidth" to 1.2 mm because I wanted to use a 0.6 mm nozzle and 0.6 mm layer width to make the grid perimeter wall half
// of the thickness of the interior cell walls. This is so that multiple grids can be stacked side-by-side to make up a larger grid, with
// the perimeter walls touching, all of the interior cell walls of the "supergrid" will have the same thickness of 1.2 mm.
//
// This should be easy to customize if you prefer thicker or thinner cell walls.
//
// GitHub repo: https://github.com/billism1/sand-matrix
//
// License: MIT

// Parameters

// The number of LEDs per row.
LEDsPerRow = 16;

// The number of LEDs per column.
LEDsPerColumn = 16;

// The center horizontal distance between LEDs in mm. This will also be the LED cube size.
LedAreaWidth = 9.95;   // Used for flexible Neopixel type matrix: 16x16, 8x8, 8x32, etc.
//LedAreaWidth = 8.15;   // Used for a hard 8x8 PCB I ordered from AliExpress. This component is not a perfect square, unfortunately.

// The center "vertical" distance between LEDs in mm. This will also be the LED cube size.
LedAreaLength = 9.95;   // Used for flexible Neopixel type matrix: 16x16, 8x8, 8x32, etc.
//LedAreaLength = 8.333;  // Used for a hard 8x8 PCB I ordered from AliExpress. This component is not a perfect square, unfortunately.

// Depth in mm.
Depth = 8;

// Wall width in mm.
WallWidth = 1.2;

// Whether or not to leave slots on the side of the grid touching the LED matrix, leaving room for the small SMD components.
SlotsForSmd = true;

// SMD slot with in mm.
SMDSlotWidth = 3.75;

// SMD slot depth in mm.
SMDSlotDepth = 1.2;

// The SMD components on the LED matrix are typically offcenter. This is how much to shift offcenter to match placement, in mm
SMDSlotShiftOffCenter = 1;   // Used for flexible Neopixel type matrix: 16x16, 8x8, 8x32, etc. the SMD capacitors are off-center under each pixel.
//SMDSlotShiftOffCenter = 0;   // Used for a hard 8x8 PCB I ordered from AliExpress - the SMD capacitors are centered under each pixel on this component.

// Whether or not to include a solid bottom layer. This can be used to 3d print a diffuser by printing a single
// (or more 2 if you want) while layer, pausing the print, and then swapping out with black filament.
IncludeDiffuser = false;

// If including a diffuser layer, this is how thick it will be in mm.
DiffuserThickness = 0.6;

GridWidth = ((LEDsPerColumn - 1) * LedAreaWidth) + (WallWidth * 2);
GridLength = ((LEDsPerRow - 1) * LedAreaLength) + (WallWidth * 2);

module LEDAreaTemplate(width, length, depth, zPosition)
{
  render()
    hull()
    {
      translate([0, 0, zPosition])
        cube([width, length, depth]);
    }
}

module SMDGap(width, length, depth, zPosition)
{
  render()
    hull()
    {
      translate([0, 0, zPosition])
        cube([width, length, depth]);
    }
}

module LedMatrix()
{
  difference()
  {
    union()
    {
      // Exterior area for each LED cube
      for (iCol = [0 : (LEDsPerColumn - 1)])
      {
        for (iRow = [0 : (LEDsPerRow - 1)])
        {
          translate([iRow * LedAreaWidth, iCol * LedAreaLength, 0])
            LEDAreaTemplate(LedAreaWidth, LedAreaLength, Depth, 0);
        }
      }
    }
  
    //color( [1, 1, 1, 1] )

    // Area to cut out for each LED cube
    for (iCol = [0 : (LEDsPerColumn - 1)])
    {
      for (iRow = [0 : (LEDsPerRow - 1)])
      {
        // Segments inside. Position slightly below 0 Z in order to "cut out" the bottom.
        translate([iRow * LedAreaWidth + (WallWidth / 2), iCol * LedAreaLength + (WallWidth / 2), 0.01])
          LEDAreaTemplate(LedAreaWidth - WallWidth, LedAreaLength - WallWidth, Depth + 0.1, (IncludeDiffuser ? DiffuserThickness : -0.1));
      }
    }

    if (SlotsForSmd)
    {
      // Create bar length-wise (for each column) to cut out slot for SMDs
      for (iRow = [0 : (LEDsPerRow - 1)])
      {
        translate([(iRow * LedAreaWidth) + (LedAreaWidth / 2) - (SMDSlotWidth / 2) - SMDSlotShiftOffCenter, WallWidth, 0.01])
          SMDGap(SMDSlotWidth, GridLength, SMDSlotDepth, Depth - SMDSlotDepth);
      }
    }
  }
}

LedMatrix();
