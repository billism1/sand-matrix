#include <Arduino.h>
#include <Math.h>
#include "FastLED.h"

#define LED_DATA_PIN 10

#define COLOR_BLACK 0

#define NUM_LEDS 256

CRGB leds[NUM_LEDS];

static const int16_t ROWS = 16;
static const int16_t COLS = 16;

int16_t inputWidth = 1;
int16_t inputX = 4;
int16_t inputY = 0;

int16_t dropCount = 0;

unsigned long lastMillis;
unsigned long maxFps = 20;
unsigned long colorChangeTime = 0;
int16_t millisToChangeColor = 250;
unsigned long inputXChangeTime = 0;
int16_t millisToChangeInputX = 6000;

int16_t BACKGROUND_COLOR = COLOR_BLACK;

byte red = 31;
byte green = 0;
byte blue = 0;
byte colorState = 0;
int16_t color = red << 11;

int16_t maxVelocity = 2;
int16_t gravity = 1;
int16_t percentInputFill = 25;
int16_t adjacentVelocityResetValue = 3;

CRGB **grid;

int16_t **velocityGrid;
int16_t **nextVelocityGrid;
int16_t **lastVelocityGrid;

int16_t **stateGrid;
int16_t **nextStateGrid;
int16_t **lastStateGrid;

static const int16_t GRID_STATE_NONE = 0;
static const int16_t GRID_STATE_NEW = 1;
static const int16_t GRID_STATE_FALLING = 2;
static const int16_t GRID_STATE_COMPLETE = 3;

int16_t maxFramesPerSecond = 2;

// Params for width and height
const uint8_t kMatrixWidth = COLS;
const uint8_t kMatrixHeight = ROWS;

// Param for different pixel layouts
const bool kMatrixSerpentineLayout = true;
const bool kMatrixVertical = false;

// XY function from:
// https://github.com/FastLED/FastLED/blob/master/examples/XYMatrix/XYMatrix.ino
uint16_t XY(uint8_t x, uint8_t y)
{
  uint16_t i;

  if (kMatrixSerpentineLayout == false)
  {
    if (kMatrixVertical == false)
    {
      i = (y * kMatrixWidth) + x;
    }
    else
    {
      i = kMatrixHeight * (kMatrixWidth - (x + 1)) + y;
    }
  }

  if (kMatrixSerpentineLayout == true)
  {
    if (kMatrixVertical == false)
    {
      if (y & 0x01)
      {
        // Odd rows run backwards
        uint8_t reverseX = (kMatrixWidth - 1) - x;
        i = (y * kMatrixWidth) + reverseX;
      }
      else
      {
        // Even rows run forwards
        i = (y * kMatrixWidth) + x;
      }
    }
    else
    { // vertical positioning
      if (x & 0x01)
      {
        i = kMatrixHeight * (kMatrixWidth - (x + 1)) + y;
      }
      else
      {
        i = kMatrixHeight * (kMatrixWidth - x) - (y + 1);
      }
    }
  }

  return i;
}

uint16_t XYsafe(uint8_t x, uint8_t y)
{
  if (x >= kMatrixWidth)
    return -1;
  if (y >= kMatrixHeight)
    return -1;
  return XY(x, y);
}

bool withinCols(int16_t value)
{
  return value >= 0 && value <= COLS - 1;
}

bool withinRows(int16_t value)
{
  return value >= 0 && value <= ROWS - 1;
}

// Color changing state machine
void setNextColor()
{
  switch (colorState)
  {
  case 0:
    green += 2;
    if (green == 64)
    {
      green = 63;
      colorState = 1;
    }
    break;
  case 1:
    red--;
    if (red == 255)
    {
      red = 0;
      colorState = 2;
    }
    break;
  case 2:
    blue++;
    if (blue == 32)
    {
      blue = 31;
      colorState = 3;
    }
    break;
  case 3:
    green -= 2;
    if (green == 255)
    {
      green = 0;
      colorState = 4;
    }
    break;
  case 4:
    red++;
    if (red == 32)
    {
      red = 31;
      colorState = 5;
    }
    break;
  case 5:
    blue--;
    if (blue == 255)
    {
      blue = 0;
      colorState = 0;
    }
    break;
  }

  color = red << 11 | green << 5 | blue;

  if (color == BACKGROUND_COLOR)
    color++;
}

void resetAdjacentPixels(int16_t x, int16_t y)
{
  int16_t xPlus = x + 1;
  int16_t xMinus = x - 1;
  int16_t yPlus = y + 1;
  int16_t yMinus = y - 1;

  // Row above
  if (withinRows(yMinus))
  {
    if (nextStateGrid[yMinus][xMinus] == GRID_STATE_COMPLETE)
    {
      nextStateGrid[yMinus][xMinus] = GRID_STATE_FALLING;
      nextVelocityGrid[yMinus][xMinus] = adjacentVelocityResetValue;
    }
    if (nextStateGrid[yMinus][x] == GRID_STATE_COMPLETE)
    {
      nextStateGrid[yMinus][x] = GRID_STATE_FALLING;
      nextVelocityGrid[yMinus][x] = adjacentVelocityResetValue;
    }
    if (nextStateGrid[yMinus][xPlus] == GRID_STATE_COMPLETE)
    {
      nextStateGrid[yMinus][xPlus] = GRID_STATE_FALLING;
      nextVelocityGrid[yMinus][xPlus] = adjacentVelocityResetValue;
    }
  }

  // Current row
  if (nextStateGrid[y][xMinus] == GRID_STATE_COMPLETE)
  {
    nextStateGrid[y][xMinus] = GRID_STATE_FALLING;
    nextVelocityGrid[y][xMinus] = adjacentVelocityResetValue;
  }
  if (nextStateGrid[y][xPlus] == GRID_STATE_COMPLETE)
  {
    nextStateGrid[y][xPlus] = GRID_STATE_FALLING;
    nextVelocityGrid[y][xPlus] = adjacentVelocityResetValue;
  }

  // Row below
  if (withinRows(yPlus))
  {
    if (nextStateGrid[yPlus][xMinus] == GRID_STATE_COMPLETE)
    {
      nextStateGrid[yPlus][xMinus] = GRID_STATE_FALLING;
      nextVelocityGrid[yPlus][xMinus] = adjacentVelocityResetValue;
    }
    if (nextStateGrid[yPlus][x] == GRID_STATE_COMPLETE)
    {
      nextStateGrid[yPlus][x] = GRID_STATE_FALLING;
      nextVelocityGrid[yPlus][x] = adjacentVelocityResetValue;
    }
    if (nextStateGrid[yPlus][xPlus] == GRID_STATE_COMPLETE)
    {
      nextStateGrid[yPlus][xPlus] = GRID_STATE_FALLING;
      nextVelocityGrid[yPlus][xPlus] = adjacentVelocityResetValue;
    }
  }
}

void setColor(CRGB *crgb, int8_t _red, int8_t _green, int8_t _blue)
{
  crgb->raw[0] = _red;   /// * `raw[0]` is the red value
  crgb->raw[1] = _green; /// * `raw[1]` is the green value
  crgb->raw[2] = _blue;  /// * `raw[2]` is the blue value
}

void resetGrid()
{
  // Serial.println("Initial values....");
  for (int16_t i = 0; i < ROWS; ++i)
  {
    velocityGrid[i] = new int16_t[COLS];
    nextVelocityGrid[i] = new int16_t[COLS];
    lastVelocityGrid[i] = new int16_t[COLS];

    stateGrid[i] = new int16_t[COLS];
    nextStateGrid[i] = new int16_t[COLS];
    lastStateGrid[i] = new int16_t[COLS];

    for (int16_t j = 0; j < COLS; ++j)
    {
      // setColor(&grid[i][j], 0, 0, 0);
      setColor(&leds[XY(j, i)], 0, 0, 0);

      velocityGrid[i][j] = 0;
      nextVelocityGrid[i][j] = 0;
      lastVelocityGrid[i][j] = 0;

      stateGrid[i][j] = GRID_STATE_NONE;
      nextStateGrid[i][j] = GRID_STATE_NONE;
      lastStateGrid[i][j] = GRID_STATE_NONE;
    }
  }
}

void setup()
{
  // Serial.begin(115200);
  // Serial.println("Hello, starting...");

  // Init 2-d array/grid to point to leds contiguous memory allocation.
  // Serial.println("Init 2-d array/grid to point to leds contiguous memory allocation....");
  grid = new CRGB *[ROWS];
  for (int16_t i = 0; i < ROWS; ++i)
  {
    grid[i] = (CRGB *)&leds[COLS * i];
  }

  // Init other 2-d arrays, holding pixel state/velocity
  // Serial.println("Init other 2-d arrays, holding pixel state/velocity....");
  velocityGrid = new int16_t *[ROWS];
  nextVelocityGrid = new int16_t *[ROWS];
  lastVelocityGrid = new int16_t *[ROWS];

  stateGrid = new int16_t *[ROWS];
  nextStateGrid = new int16_t *[ROWS];
  lastStateGrid = new int16_t *[ROWS];

  // Initial values
  resetGrid();

  // Serial.println("Init FastLED....");
  FastLED.addLeds<NEOPIXEL, LED_DATA_PIN>(leds, NUM_LEDS);

  colorChangeTime = millis() + 1000;

  lastMillis = millis();
}

void loop()
{
  // Serial.println("Looping...");

  unsigned long currentMillis = millis();
  unsigned long diffMillis = currentMillis - lastMillis;

  if ((1000 / maxFps) > diffMillis)
  {
    return;
  }

  lastMillis = currentMillis;
  // Serial.println("Continuing loop...");

  // Change the color of the pixels over time
  if (colorChangeTime < millis())
  {
    colorChangeTime = millis() + millisToChangeColor;
    setNextColor();
  }

  // Change the inputX of the pixels over time or if the current input is already filled.
  if (inputXChangeTime < millis() || stateGrid[inputY][inputX] != GRID_STATE_NONE)
  {
    inputXChangeTime = millis() + millisToChangeInputX;
    inputX = random(0, COLS - 1);
  }

  // Randomly add an area of pixels
  int16_t halfInputWidth = inputWidth / 2;
  for (int16_t i = -halfInputWidth; i <= halfInputWidth; ++i)
  {
    for (int16_t j = -halfInputWidth; j <= halfInputWidth; ++j)
    {
      if (random(100) < percentInputFill)
      {
        dropCount++;
        if (dropCount > (inputWidth * ROWS * COLS))
        {
          dropCount = 0;
          resetGrid();
        }

        int16_t col = inputX + i;
        int16_t row = inputY + j;

        if (withinCols(col) && withinRows(row) &&
            (stateGrid[row][col] == GRID_STATE_NONE || stateGrid[row][col] == GRID_STATE_COMPLETE))
        {
          // setColor(&grid[row][col], red, green, blue);
          setColor(&leds[XY(col, row)], red, green, blue);
          velocityGrid[row][col] = 1;
          stateGrid[row][col] = GRID_STATE_NEW;
        }
      }
    }
  }

  // // DEBUG
  // // DEBUG
  // Serial.println("Array Values:");
  // for (int16_t i = 0; i < ROWS; ++i)
  // {
  //   for (int16_t j = 0; j < COLS; ++j)
  //   {
  //     Serial.printf("|%02hhX%02hhX%02hhX|", grid[i][j].red, grid[i][j].green, grid[i][j].blue);
  //   }
  //   Serial.println("");
  // }
  // // DEBUG
  // // DEBUG

  // Draw the pixels
  FastLED.show();

  // Clear the grids for the next frame of animation
  for (int16_t i = 0; i < ROWS; ++i)
  {
    for (int16_t j = 0; j < COLS; ++j)
    {
      nextVelocityGrid[i][j] = 0;
      nextStateGrid[i][j] = GRID_STATE_NONE;
    }
  }

  // Serial.println("Checking every cell...");
  // unsigned long beforeCheckEveryCell = millis();
  // unsigned long pixelsChecked = 0;
  // Check every pixel to see which need moving, and move them.
  for (int16_t i = 0; i < ROWS; ++i)
  {
    for (int16_t j = 0; j < COLS; ++j)
    {
      // This nexted loop is where the bulk of the computations occur.
      // Tread lightly in here, and check as few pixels as needed.

      // Get the state of the current pixel.
      // CRGB pixelColor = grid[i][j];
      CRGB pixelColor = leds[XY(j, i)];
      int16_t pixelVelocity = velocityGrid[i][j];
      int16_t pixelState = stateGrid[i][j];

      bool moved = false;

      // If the current pixel has landed, no need to keep checking for its next move.
      if (pixelState != GRID_STATE_NONE && pixelState != GRID_STATE_COMPLETE)
      {
        // pixelsChecked++;

        int16_t newPos = int16_t(i + min(maxVelocity, pixelVelocity));
        for (int16_t y = newPos; y > i; y--)
        {
          if (!withinRows(y))
          {
            continue;
          }

          int16_t belowState = stateGrid[y][j];
          int16_t belowNextState = nextStateGrid[y][j];

          int16_t direction = 1;
          if (random(100) < 50)
          {
            direction *= -1;
          }

          int16_t belowStateA = -1;
          int16_t belowNextStateA = -1;
          int16_t belowStateB = -1;
          int16_t belowNextStateB = -1;

          if (withinCols(j + direction))
          {
            belowStateA = stateGrid[y][j + direction];
            belowNextStateA = nextStateGrid[y][j + direction];
          }
          if (withinCols(j - direction))
          {
            belowStateB = stateGrid[y][j - direction];
            belowNextStateB = nextStateGrid[y][j - direction];
          }

          if (belowState == GRID_STATE_NONE && belowNextState == GRID_STATE_NONE)
          {
            // This pixel will go straight down.
            // grid[y][j] = pixelColor;
            leds[XY(j, y)] = pixelColor;
            nextVelocityGrid[y][j] = pixelVelocity + gravity;
            nextStateGrid[y][j] = GRID_STATE_FALLING;
            moved = true;
            break;
          }
          else if (belowStateA == GRID_STATE_NONE && belowNextStateA == GRID_STATE_NONE)
          {
            // This pixel will fall to side A (right)
            // grid[y][j + direction] = pixelColor;
            leds[XY(j + direction, y)] = pixelColor;
            nextVelocityGrid[y][j + direction] = pixelVelocity + gravity;
            nextStateGrid[y][j + direction] = GRID_STATE_FALLING;
            moved = true;
            break;
          }
          else if (belowStateB == GRID_STATE_NONE && belowNextStateB == GRID_STATE_NONE)
          {
            // This pixel will fall to side B (left)
            // grid[y][j - direction] = pixelColor;
            leds[XY(j - direction, y)] = pixelColor;
            nextVelocityGrid[y][j - direction] = pixelVelocity + gravity;
            nextStateGrid[y][j - direction] = GRID_STATE_FALLING;
            moved = true;
            break;
          }
        }
      }

      if (moved)
      {
        // Reset color where this pixel was.
        // setColor(&grid[i][j], 0, 0, 0);
        setColor(&leds[XY(j, i)], 0, 0, 0);

        resetAdjacentPixels(j, i);
      }

      if (pixelState != GRID_STATE_NONE && !moved)
      {
        nextVelocityGrid[i][j] = velocityGrid[i][j] + gravity;
        if (pixelState == GRID_STATE_NEW)
          nextStateGrid[i][j] = GRID_STATE_FALLING;
        else if (pixelState == GRID_STATE_FALLING && pixelVelocity > 2)
          nextStateGrid[i][j] = GRID_STATE_COMPLETE;
        else
          nextStateGrid[i][j] = pixelState; // should be GRID_STATE_COMPLETE
      }
    }
  }
  // unsigned long afterCheckEveryCell = millis();
  // unsigned long checkCellTime = afterCheckEveryCell - beforeCheckEveryCell;
  // Serial.printf("%lu pixels checked in %lu ms.\r\n", pixelsChecked, checkCellTime);

  // Swap the state and velocity grid pointers.

  lastVelocityGrid = velocityGrid;
  lastStateGrid = stateGrid;

  velocityGrid = nextVelocityGrid;
  stateGrid = nextStateGrid;

  nextVelocityGrid = lastVelocityGrid;
  nextStateGrid = lastStateGrid;
}
