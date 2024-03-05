// License: MIT
//
// This is for generating grid support for Neopixel matrices (ws8212b LED panels).
// The support can be 3d printed and used to hold a diffuser in front of the LED panel.

// Parameters

// The number of LEDs per row.
LEDsPerRow = 16;

// The number of LEDs per column.
LEDsPerColumn = 16;

// The center distance between LEDs in mm. This will also be the LED cube size.
LedAreaWidth = 9.95;
 
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
SMDSlotShiftOffCenter = 1;

// Whether or not to include a solid bottom layer. This can be used to 3d print a diffuser by printing a single
// (or more 2 if you want) while layer, pausing the print, and then swapping out with black filament.
IncludeDiffuser = false;

// If including a diffuser layer, this is how thick it will be in mm.
DiffuserThickness = 0.6;

GridLength = ((LEDsPerColumn - 1) * LedAreaWidth) + (WallWidth * 2);

module LEDAreaTemplate(width, depth, zPosition)
{
  render()
    hull()
    {
      translate([0, 0, zPosition])
        cube([width, width, depth]);
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
					translate([iRow * LedAreaWidth, iCol * LedAreaWidth, 0])
						LEDAreaTemplate(LedAreaWidth, Depth, 0);
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
				translate([iRow * LedAreaWidth + (WallWidth / 2), iCol * LedAreaWidth + (WallWidth / 2), 0.01])
          LEDAreaTemplate(LedAreaWidth - WallWidth, Depth + 0.1, (IncludeDiffuser ? DiffuserThickness : -0.1));
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
