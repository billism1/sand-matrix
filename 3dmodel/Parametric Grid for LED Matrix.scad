// Parameters

// The number of LEDs per row
LEDsPerRow = 16;

// The number of LEDs per column
LEDsPerColumn = 16;

// The center distance between LEDs in mm (This will also be the LED cube size)
LedDistance = 9;

// Depth in mm
Depth = 8;

// Wall width in mm
WallWidth = 1;

// Whether or not to leave slots on the side of the grid touching the LED matrix, leaving room for the small SMD components
SlotsForSmd = true;

// SMD slot with in mm
SMDSlotWidth = 3;

// SMD slot depth in mm
SMDSlotDepth = 1;

// The SMD components on the LED matrix are typically offcenter. This is how much to shift offcenter to match placement, in mm
SMDSlotShiftOffCenter = 1;

GridWidth = ((LEDsPerRow - 1) * LedDistance) + (WallWidth * 2);

GridLength = ((LEDsPerColumn - 1) * LedDistance) + (WallWidth * 2);

module LEDAreaTemplate(width, depth, zPosition)
{
  render()
    hull()
    {
      translate([-width / 2, -width / 2, zPosition])
        cube([width, width, depth]);
    }
}

module SMDGap(width, length, depth, zPosition)
{
  render()
    hull()
    {
      translate([-width / 2, -length / 2, zPosition])
        cube([width, length, depth]);
    }
}

module LedMatrix()
{
  // Center on canvas
  lastRowLEDOffset = (((LEDsPerRow - 1) * LedDistance) / 2) * -1;
  lastColumnLEDOffset = (((LEDsPerColumn - 1) * LedDistance) / 2) / -1;
  
	difference()
	{
		union()
		{
			// Exterior area for each LED cube
			for (iRow = [0 : (LEDsPerRow - 1)])
      {
				for (iCol = [0 : (LEDsPerColumn - 1)])
				{
					translate([lastColumnLEDOffset + iCol * LedDistance, lastRowLEDOffset + iRow * LedDistance, 0])
						LEDAreaTemplate(LedDistance + WallWidth - 1, Depth, 0);
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
				translate([lastColumnLEDOffset + iCol * LedDistance, lastRowLEDOffset + iRow * LedDistance, 0.01])
          LEDAreaTemplate(LedDistance - WallWidth, Depth + 0.1, -0.1);
			}
    }

    if (SlotsForSmd)
    {
      // Create bar length-wise (for each column) to cut out slot for SMDs
      for (iCol = [0 : (LEDsPerColumn - 1)])
      {
        translate([(lastColumnLEDOffset + iCol * LedDistance) + SMDSlotShiftOffCenter, 0, 0.01])
          SMDGap(SMDSlotWidth, GridLength, SMDSlotDepth, Depth - SMDSlotDepth);
      }
    }
	}
}

LedMatrix();
