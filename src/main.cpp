#include <Arduino.h>
#include <Math.h>
#include "FastLED.h"
#include "sin_wave_tech.h"

struct GridState
{
  uint16_t state;
  uint16_t kValue;
  int16_t velocity;
};

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
unsigned long allColorChangeTime = 0;
int16_t millisToChangeColor = 250;
int16_t millisToChangeAllColors = 150;
unsigned long inputXChangeTime = 0;
int16_t millisToChangeInputX = 6000;

int16_t BACKGROUND_COLOR = COLOR_BLACK;

byte rgbValues[] = {0x31, 0x00, 0x00}; // red, green, blue
#define COLORS_ARRAY_RED rgbValues[0]
#define COLORS_ARRAY_GREEN rgbValues[1]
#define COLORS_ARRAY_BLUE rgbValues[2]
// byte red = 31;
// byte green = 0;
// byte blue = 0;
uint16_t newKValue = 0;

int16_t maxVelocity = 2;
int16_t gravity = 1;
int16_t percentInputFill = 25;
int16_t adjacentVelocityResetValue = 3;

GridState **stateGrid;
GridState **nextStateGrid;
GridState **lastStateGrid;

static const uint16_t GRID_STATE_NONE = 0;
static const uint16_t GRID_STATE_NEW = 1;
static const uint16_t GRID_STATE_FALLING = 2;
static const uint16_t GRID_STATE_COMPLETE = 3;

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
void setNextColor(byte *_rgbValues, uint16_t &kValue)
{
  switch (kValue)
  {
  case 0:
    _rgbValues[1] += 2;
    if (_rgbValues[1] == 64)
    {
      _rgbValues[1] = 63;
      kValue = (uint16_t)1;
    }
    break;
  case 1:
    _rgbValues[0]--;
    if (_rgbValues[0] == 255)
    {
      _rgbValues[0] = 0;
      kValue = (uint16_t)2;
    }
    break;
  case 2:
    _rgbValues[2]++;
    if (_rgbValues[2] == 32)
    {
      _rgbValues[2] = 31;
      kValue = (uint16_t)3;
    }
    break;
  case 3:
    _rgbValues[1] -= 2;
    if (_rgbValues[1] == 255)
    {
      _rgbValues[1] = 0;
      kValue = (uint16_t)4;
    }
    break;
  case 4:
    _rgbValues[0]++;
    if (_rgbValues[0] == 32)
    {
      _rgbValues[0] = 31;
      kValue = (uint16_t)5;
    }
    break;
  case 5:
    _rgbValues[2]--;
    if (_rgbValues[2] == 255)
    {
      _rgbValues[2] = 0;
      kValue = (uint16_t)0;
    }
    break;
  }
}

void setNextColorAll()
{
  for (int16_t i = 0; i < ROWS; ++i)
  {
    for (int16_t j = 0; j < COLS; ++j)
    {
      if (stateGrid[i][j].state != GRID_STATE_NONE)
      {
        setNextColor(leds[XY(j, i)].raw, stateGrid[i][j].kValue);
      }
    }
  }
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
    if (nextStateGrid[yMinus][xMinus].state == GRID_STATE_COMPLETE)
    {
      nextStateGrid[yMinus][xMinus].state = GRID_STATE_FALLING;
      nextStateGrid[yMinus][xMinus].velocity = adjacentVelocityResetValue;
    }
    if (nextStateGrid[yMinus][x].state == GRID_STATE_COMPLETE)
    {
      nextStateGrid[yMinus][x].state = GRID_STATE_FALLING;
      nextStateGrid[yMinus][x].velocity = adjacentVelocityResetValue;
    }
    if (nextStateGrid[yMinus][xPlus].state == GRID_STATE_COMPLETE)
    {
      nextStateGrid[yMinus][xPlus].state = GRID_STATE_FALLING;
      nextStateGrid[yMinus][xPlus].velocity = adjacentVelocityResetValue;
    }
  }

  // Current row
  if (nextStateGrid[y][xMinus].state == GRID_STATE_COMPLETE)
  {
    nextStateGrid[y][xMinus].state = GRID_STATE_FALLING;
    nextStateGrid[y][xMinus].velocity = adjacentVelocityResetValue;
  }
  if (nextStateGrid[y][xPlus].state == GRID_STATE_COMPLETE)
  {
    nextStateGrid[y][xPlus].state = GRID_STATE_FALLING;
    nextStateGrid[y][xPlus].velocity = adjacentVelocityResetValue;
  }

  // Row below
  if (withinRows(yPlus))
  {
    if (nextStateGrid[yPlus][xMinus].state == GRID_STATE_COMPLETE)
    {
      nextStateGrid[yPlus][xMinus].state = GRID_STATE_FALLING;
      nextStateGrid[yPlus][xMinus].velocity = adjacentVelocityResetValue;
    }
    if (nextStateGrid[yPlus][x].state == GRID_STATE_COMPLETE)
    {
      nextStateGrid[yPlus][x].state = GRID_STATE_FALLING;
      nextStateGrid[yPlus][x].velocity = adjacentVelocityResetValue;
    }
    if (nextStateGrid[yPlus][xPlus].state == GRID_STATE_COMPLETE)
    {
      nextStateGrid[yPlus][xPlus].state = GRID_STATE_FALLING;
      nextStateGrid[yPlus][xPlus].velocity = adjacentVelocityResetValue;
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
    stateGrid[i] = new GridState[COLS];
    nextStateGrid[i] = new GridState[COLS];
    lastStateGrid[i] = new GridState[COLS];

    for (int16_t j = 0; j < COLS; ++j)
    {
      setColor(&leds[XY(j, i)], 0, 0, 0);

      stateGrid[i][j].state = GRID_STATE_NONE;
      stateGrid[i][j].velocity = 0;
      stateGrid[i][j].kValue = 0;
      nextStateGrid[i][j].state = GRID_STATE_NONE;
      nextStateGrid[i][j].velocity = 0;
      nextStateGrid[i][j].kValue = 0;
      lastStateGrid[i][j].state = GRID_STATE_NONE;
      lastStateGrid[i][j].velocity = 0;
      lastStateGrid[i][j].kValue = 0;
    }
  }
}

void setup()
{
  // Serial.begin(115200);
  // Serial.println("Hello, starting...");

  // Init other 2-d arrays, holding pixel state
  // Serial.println("Init other 2-d arrays, holding pixel state....");
  stateGrid = new GridState *[ROWS];
  nextStateGrid = new GridState *[ROWS];
  lastStateGrid = new GridState *[ROWS];

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

  // Change the color of the new pixels over time
  if (colorChangeTime < millis())
  {
    colorChangeTime = millis() + millisToChangeColor;
    setNextColor(rgbValues, newKValue);
  }

  // Change the color of the fallen pixels over time
  if (allColorChangeTime < millis())
  {
    allColorChangeTime = millis() + millisToChangeAllColors;
    setNextColorAll();
  }

  unsigned long currentMillis = millis();
  unsigned long diffMillis = currentMillis - lastMillis;

  if ((1000 / maxFps) > diffMillis)
  {
    return;
  }

  lastMillis = currentMillis;
  // Serial.println("Continuing loop...");

  // Change the inputX of the pixels over time or if the current input is already filled.
  if (inputXChangeTime < millis() || stateGrid[inputY][inputX].state != GRID_STATE_NONE)
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
            (stateGrid[row][col].state == GRID_STATE_NONE || stateGrid[row][col].state == GRID_STATE_COMPLETE))
        {
          setColor(&leds[XY(col, row)], COLORS_ARRAY_RED, COLORS_ARRAY_GREEN, COLORS_ARRAY_BLUE);
          stateGrid[row][col].state = GRID_STATE_NEW;
          stateGrid[row][col].velocity = 1;
          stateGrid[row][col].kValue = newKValue;
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
  //     Serial.printf("|%02hhX%02hhX%02hhX|", stateGrid[i][j].state, stateGrid[i][j].state, stateGrid[i][j].state);
  //   }
  //   Serial.println("");
  // }
  // // DEBUG
  // // DEBUG

  // Draw the pixels
  FastLED.show();

  // Clear the next state frame of animation
  for (int16_t i = 0; i < ROWS; ++i)
  {
    for (int16_t j = 0; j < COLS; ++j)
    {
      nextStateGrid[i][j].state = GRID_STATE_NONE;
      nextStateGrid[i][j].velocity = 0;
      nextStateGrid[i][j].kValue = 0;
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
      CRGB pixelColor = leds[XY(j, i)];
      uint16_t pixelState = stateGrid[i][j].state;
      int16_t pixelVelocity = stateGrid[i][j].velocity;
      uint16_t pixelKValue = stateGrid[i][j].kValue;

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

          GridState belowState = stateGrid[y][j];
          GridState belowNextState = nextStateGrid[y][j];

          int16_t direction = 1;
          if (random(100) < 50)
          {
            direction *= -1;
          }

          GridState *belowStateA = NULL;
          GridState *belowNextStateA = NULL;
          GridState *belowStateB = NULL;
          GridState *belowNextStateB = NULL;

          if (withinCols(j + direction))
          {
            belowStateA = &stateGrid[y][j + direction];
            belowNextStateA = &nextStateGrid[y][j + direction];
          }
          if (withinCols(j - direction))
          {
            belowStateB = &stateGrid[y][j - direction];
            belowNextStateB = &nextStateGrid[y][j - direction];
          }

          if (belowState.state == GRID_STATE_NONE && belowNextState.state == GRID_STATE_NONE)
          {
            // This pixel will go straight down.
            leds[XY(j, y)] = pixelColor;
            nextStateGrid[y][j].state = GRID_STATE_FALLING;
            nextStateGrid[y][j].velocity = pixelVelocity + gravity;
            nextStateGrid[y][j].kValue = pixelKValue;
            moved = true;
            break;
          }
          else if ((belowStateA != NULL && belowStateA->state == GRID_STATE_NONE) && (belowNextStateA != NULL && belowNextStateA->state == GRID_STATE_NONE))
          {
            // This pixel will fall to side A (right)
            leds[XY(j + direction, y)] = pixelColor;
            nextStateGrid[y][j + direction].state = GRID_STATE_FALLING;
            nextStateGrid[y][j + direction].velocity = pixelVelocity + gravity;
            nextStateGrid[y][j + direction].kValue = pixelKValue;
            moved = true;
            break;
          }
          else if ((belowStateB != NULL && belowStateB->state == GRID_STATE_NONE) && (belowNextStateB != NULL && belowNextStateB->state == GRID_STATE_NONE))
          {
            // This pixel will fall to side B (left)
            leds[XY(j - direction, y)] = pixelColor;
            nextStateGrid[y][j - direction].state = GRID_STATE_FALLING;
            nextStateGrid[y][j - direction].velocity = pixelVelocity + gravity;
            nextStateGrid[y][j - direction].kValue = pixelKValue;
            moved = true;
            break;
          }
        }
      }

      if (moved)
      {
        // Reset color where this pixel was.
        setColor(&leds[XY(j, i)], 0, 0, 0);

        resetAdjacentPixels(j, i);
      }

      if (pixelState != GRID_STATE_NONE && !moved)
      {
        nextStateGrid[i][j].velocity = pixelVelocity + gravity;
        nextStateGrid[i][j].kValue = pixelKValue;

        if (pixelState == GRID_STATE_NEW)
          nextStateGrid[i][j].state = GRID_STATE_FALLING;
        else if (pixelState == GRID_STATE_FALLING && pixelVelocity > 2)
          nextStateGrid[i][j].state = GRID_STATE_COMPLETE;
        else
          nextStateGrid[i][j].state = pixelState; // should be GRID_STATE_COMPLETE
      }
    }
  }
  // unsigned long afterCheckEveryCell = millis();
  // unsigned long checkCellTime = afterCheckEveryCell - beforeCheckEveryCell;
  // Serial.printf("%lu pixels checked in %lu ms.\r\n", pixelsChecked, checkCellTime);

  // Swap the state pointers.

  lastStateGrid = stateGrid;
  stateGrid = nextStateGrid;
  nextStateGrid = lastStateGrid;
}
