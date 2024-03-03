// Parameters

// The number of LEDs per row
LEDsPerRow = 5;

// The number of LEDs per column
LEDsPerColumn = 5;

// The center distance between LEDs in mm (This will also be the LED cube size)
LedAreaWidth = 9.95;
 
// Depth in mm
//Depth = 8;
Depth = 4;

// Wall width in mm
WallWidth = 1.2;

// Whether or not to leave slots on the side of the grid touching the LED matrix, leaving room for the small SMD components
SlotsForSmd = true;

// SMD slot with in mm
SMDSlotWidth = 3;

// SMD slot depth in mm
SMDSlotDepth = 1;

// The SMD components on the LED matrix are typically offcenter. This is how much to shift offcenter to match placement, in mm
SMDSlotShiftOffCenter = 1;

GridLength = ((LEDsPerRow - 1) * LedAreaWidth) + (WallWidth * 2);

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
			for (iRow = [0 : (LEDsPerRow - 1)])
      {
				for (iCol = [0 : (LEDsPerColumn - 1)])
				{
					translate([iCol * LedAreaWidth, iRow * LedAreaWidth, 0])
						LEDAreaTemplate(LedAreaWidth, Depth, 0);
				}
      }
		}
	
		//color( [1, 1, 1, 1] )

    // Area to cut out for each LED cube
		for (iRow = [0 : (LEDsPerRow - 1)])
    {
			for (iCol = [0 : (LEDsPerColumn - 1)])
			{
				// Segments inside. Position slightly below 0 Z in order to "cut out" the bottom.
				translate([iCol * LedAreaWidth + (WallWidth / 2), iRow * LedAreaWidth + (WallWidth / 2), 0.01])
          LEDAreaTemplate(LedAreaWidth - WallWidth, Depth + 0.1, -0.1);
			}
    }

    if (SlotsForSmd)
    {
      yTranslateOffset = (((LEDsPerRow - 1) * LedAreaWidth) / 2);
      // Create bar length-wise (for each column) to cut out slot for SMDs
      for (iCol = [0 : (LEDsPerColumn - 1)])
      {
        translate([(iCol * LedAreaWidth) + (LedAreaWidth / 2) - (SMDSlotWidth / 2) - SMDSlotShiftOffCenter, WallWidth, 0.01])
          SMDGap(SMDSlotWidth, GridLength, SMDSlotDepth, Depth - SMDSlotDepth);
      }
    }
	}
}

LedMatrix();
