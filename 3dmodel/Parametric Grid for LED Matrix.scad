// Parameters

// The number of LEDs per row
LEDsPerRow = 16;

// The number of LEDs per column
LEDsPerColumn = 16;

// The center distance between LEDs in mm (This will also be the LED cube size)
LedDistance = 9;

// Depth in mm
Depth = 10;

// Wall width in mm
WallWidth = 1;

GridWidth = ((LEDsPerRow - 1) * LedDistance) + (WallWidth * 2);

GridHeight = ((LEDsPerColumn - 1) * LedDistance) + (WallWidth * 2);

module SegTemplate(width, depth, zPosition)
{
  render()
    // Cubes
    hull()
    {
      translate([-width/2, -width/2, zPosition])
        cube([width, width, depth]);
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
			for ( iRow = [0 : (LEDsPerRow - 1)] )
      {
				for ( iCol = [0 : (LEDsPerColumn - 1)] )
				{
					translate([lastColumnLEDOffset + iCol * LedDistance, lastRowLEDOffset + iRow * LedDistance, 0])
						SegTemplate(LedDistance + WallWidth - 1, Depth, 0);
				}
      }
		}
	
    // Area to cut out for each LED cube
		color( [1, 1, 1, 1] )
		for ( iRow = [0 : (LEDsPerRow - 1)] )
    {
			for ( iCol = [0 : (LEDsPerColumn - 1)] )
			{
				// Segments inside. Position slightly below 0 Z in order to "cut out" the bottom.
				translate([lastColumnLEDOffset + iCol * LedDistance, lastRowLEDOffset + iRow * LedDistance, 0.01])
          SegTemplate(LedDistance - WallWidth, Depth + 0.1, -0.1);
			}
    }
	}
}

LedMatrix();
