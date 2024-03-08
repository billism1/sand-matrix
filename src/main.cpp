#include <Arduino.h>
#include <Math.h>
#include "FastLED.h"
#include "colorChangeRoutine.h"

//////////////////////////////////////////
// Parameters you can play with:

int16_t millisToChangeColor = 250;
int16_t millisToChangeAllColors = 150;
int16_t millisToChangeInputX = 6000;

int16_t inputWidth = 1;
int16_t inputX = 4;
int16_t inputY = 0;
int16_t percentInputFill = 20;

// Maximum frames per second.
// The high the value, the faster the pixels fall.
unsigned long maxFps = 20;

int16_t maxVelocity = 2;
int16_t gravity = 1;
int16_t adjacentVelocityResetValue = 3;

// End parameters you can play with
//////////////////////////////////////////

// Param for different pixel layouts
const bool perPanelIsSerpentineLayout = true;
const bool perPanelIsVertical = false;

//////////////////////////////////////////
// Display size parameters:

#define LED_PANELS_1
// #define LED_PANELS_9_16x16

#ifdef LED_PANELS_1
static const uint16_t ROWS = 16;
static const uint16_t COLS = 16;
static const uint16_t LEDS_PER_PIN = (ROWS * COLS);

// Params for width and height when translating X/Y values to LED pointer.
const uint16_t perPanelWidth = COLS;
const uint16_t perPanelHeight = ROWS;
#elif defined(LED_PANELS_9_16x16)
// 3 vertical 16x16 panels, making 1 contiguous 16x48 panels each.
static const uint16_t ROWS = 48;
static const uint16_t COLS = 48;

static const uint16_t LEDS_PER_PIN = (ROWS * COLS) / 3;

// Params for width and height when translating X/Y values to LED pointer.
const uint16_t perPanelWidth = COLS / 3;
const uint16_t perPanelHeight = ROWS;
#endif

static const uint16_t NUM_LEDS = ROWS * COLS;

// Display size parameters
//////////////////////////////////////////

struct GridState
{
  uint16_t state;
  uint16_t kValue;
  int16_t velocity;
};

#ifndef LED_DATA_PIN_PANEL_1
#define LED_DATA_PIN_PANEL_1 12
#endif
#ifndef LED_DATA_PIN_PANEL_2
#define LED_DATA_PIN_PANEL_2 13
#endif
#ifndef LED_DATA_PIN_PANEL_3
#define LED_DATA_PIN_PANEL_3 14
#endif

#define COLOR_BLACK 0

CRGB *leds;

int16_t dropCount = 0;

unsigned long lastMillis;
unsigned long colorChangeTime = 0;
unsigned long allColorChangeTime = 0;

unsigned long inputXChangeTime = 0;

int16_t BACKGROUND_COLOR = COLOR_BLACK;

byte rgbValues[] = {0x31, 0x00, 0x00}; // red, green, blue
#define COLORS_ARRAY_RED rgbValues[0]
#define COLORS_ARRAY_GREEN rgbValues[1]
#define COLORS_ARRAY_BLUE rgbValues[2]

uint16_t newKValue = 0;

GridState **stateGrid;
GridState **nextStateGrid;
GridState **lastStateGrid;

static const uint16_t GRID_STATE_NONE = 0;
static const uint16_t GRID_STATE_NEW = 1;
static const uint16_t GRID_STATE_FALLING = 2;
static const uint16_t GRID_STATE_COMPLETE = 3;

// XY function from:
// https://github.com/FastLED/FastLED/blob/master/examples/XYMatrix/XYMatrix.ino
// then modified here.
uint16_t getPanelXYOffset(uint16_t x, uint16_t y)
{
  uint16_t i;

  if (perPanelIsSerpentineLayout == false)
  {
    if (perPanelIsVertical == false)
    {
      i = (y * perPanelWidth) + x;
    }
    else
    {
      i = perPanelHeight * (perPanelWidth - (x + 1)) + y;
    }
  }

  if (perPanelIsSerpentineLayout == true)
  {
    if (perPanelIsVertical == false)
    {
      if (y & 0x01)
      {
        // Odd rows run backwards
        uint16_t reverseX = (perPanelWidth - 1) - x;
        i = (y * perPanelWidth) + reverseX;
      }
      else
      {
        // Even rows run forwards
        i = (y * perPanelWidth) + x;
      }
    }
    else
    { // vertical positioning
      if (x & 0x01)
      {
        i = perPanelHeight * (perPanelWidth - (x + 1)) + y;
      }
      else
      {
        i = perPanelHeight * (perPanelWidth - x) - (y + 1);
      }
    }
  }

  return i;
}

// uint16_t getPanelXYOffsetsafe(uint16_t x, uint16_t y)
// {
//   if (x >= perPanelWidth)
//     return -1;
//   if (y >= perPanelHeight)
//     return -1;
//   return getPanelXYOffset(x, y);
// }

// x and y are the coordinates for the entier matrix, made up of other panels.
// If needed, the conversion to get the correct led array address will be done.
CRGB *getCrgb(uint16_t xCol, uint16_t yRow)
{
#ifdef LED_PANELS_1
  return &leds[getPanelXYOffset(xCol, yRow)];
#elif defined(LED_PANELS_9_16x16)

  uint16_t ledOffset = 0;
  uint16_t newXCol = xCol;

  // Get the correct 1 of the 3 pins and set the color,
  if (xCol >= perPanelWidth * 2)
  {
    // Offset for pin 3
    ledOffset = LEDS_PER_PIN * 2;
    newXCol = xCol - (perPanelWidth * 2);
  }
  else if (xCol >= perPanelWidth)
  {
    // Offset for pin 2
    ledOffset = LEDS_PER_PIN;
    newXCol = xCol - perPanelWidth;
  }
  else
  {
    // Offset for pin 1 (0)
    // ledOffset = 0;
    // xCol = xCol
  }

  return &leds[ledOffset + getPanelXYOffset(newXCol, yRow)];
#endif
}

void setNextColor(uint16_t xCol, uint16_t yRow, uint16_t &kValue)
{
  CRGB *crgb = getCrgb(xCol, yRow);
  setNextColor(crgb->raw, kValue);
}

bool withinCols(int16_t value)
{
  return value >= 0 && value <= COLS - 1;
}

bool withinRows(int16_t value)
{
  return value >= 0 && value <= ROWS - 1;
}

void setNextColorAll()
{
  for (int16_t i = 0; i < ROWS; ++i)
  {
    for (int16_t j = 0; j < COLS; ++j)
    {
      if (stateGrid[i][j].state != GRID_STATE_NONE)
      {
        // setNextColor(leds[getPanelXYOffset(j, i)].raw, stateGrid[i][j].kValue);
        setNextColor(j, i, stateGrid[i][j].kValue);
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

void setColor(uint16_t xCol, uint16_t yRow, int8_t _red, int8_t _green, int8_t _blue)
{
  CRGB *crgb = getCrgb(xCol, yRow);

  crgb->raw[0] = _red;   /// * `raw[0]` is the red value
  crgb->raw[1] = _green; /// * `raw[1]` is the green value
  crgb->raw[2] = _blue;  /// * `raw[2]` is the blue value
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
  for (uint16_t i = 0; i < ROWS; ++i)
  {
    for (uint16_t j = 0; j < COLS; ++j)
    {
      // setColor(&leds[getPanelXYOffset(j, i)], 0, 0, 0);
      setColor(getCrgb(j, i), 0, 0, 0);

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

void setupFastLED_1_Panel()
{
  leds = new CRGB[NUM_LEDS];

  FastLED.addLeds<NEOPIXEL, LED_DATA_PIN_PANEL_1>(leds, NUM_LEDS);
}

void setupFastLED_3_Panels_16x48()
{
  leds = new CRGB[NUM_LEDS];

  FastLED.addLeds<NEOPIXEL, LED_DATA_PIN_PANEL_1>(leds, LEDS_PER_PIN);
  FastLED.addLeds<NEOPIXEL, LED_DATA_PIN_PANEL_2>(leds + LEDS_PER_PIN, LEDS_PER_PIN);
  FastLED.addLeds<NEOPIXEL, LED_DATA_PIN_PANEL_3>(leds + LEDS_PER_PIN * 2, LEDS_PER_PIN);
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Hello, starting...");
  Serial.printf("Pins used for LED strip output: %d, %d, %d\n", LED_DATA_PIN_PANEL_1, LED_DATA_PIN_PANEL_2, LED_DATA_PIN_PANEL_3);

  // Init 2-d arrays, holding pixel state
  // Serial.println("Init other 2-d arrays, holding pixel state....");
  stateGrid = new GridState *[ROWS];
  nextStateGrid = new GridState *[ROWS];
  lastStateGrid = new GridState *[ROWS];

  for (int16_t i = 0; i < ROWS; ++i)
  {
    stateGrid[i] = new GridState[COLS];
    nextStateGrid[i] = new GridState[COLS];
    lastStateGrid[i] = new GridState[COLS];
  }

  // Serial.println("Init FastLED....");
#ifdef LED_PANELS_1
  setupFastLED_1_Panel();
#elif defined(LED_PANELS_9_16x16)
  setupFastLED_3_Panels_16x48();
#endif

  // Initial values
  resetGrid();

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
  // Serial.println("Looping within FPS limit...");

  // Change the inputX of the pixels over time or if the current input is already filled.
  if (inputXChangeTime < millis() || stateGrid[inputY][inputX].state != GRID_STATE_NONE)
  {
    inputXChangeTime = millis() + millisToChangeInputX;
    inputX = random(0, COLS);
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
          setColor(getCrgb(col, row), COLORS_ARRAY_RED, COLORS_ARRAY_GREEN, COLORS_ARRAY_BLUE);
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
  // for (int16_t yRow = 0; yRow < ROWS; ++yRow)
  // {
  //   for (int16_t xCol = 0; xCol < COLS; ++xCol)
  //   {
  //     // Serial.printf("|%02hhX|", stateGrid[yRow][xCol].state);
  //     auto thisColor = getCrgb(xCol, yRow);
  //     Serial.printf("|%02hhX%02hhX%02hhX|", thisColor->raw[0], thisColor->raw[1], thisColor->raw[2]);
  //   }
  //   Serial.println("");
  // }
  // // DEBUG
  // // DEBUG

  // Draw the pixels
  FastLED.show();

  // Clear the next state frame of animation
  for (uint16_t i = 0; i < ROWS; ++i)
  {
    for (uint16_t j = 0; j < COLS; ++j)
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
      CRGB pixelColor = *getCrgb(j, i);
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
            // leds[getPanelXYOffset(j, y)] = pixelColor;
            CRGB *crgb = getCrgb(j, y);
            // TODO: VERIFY THIS WORKS or use: *crgb = pixelColor;:
            // TODO: VERIFY THIS WORKS or use: *crgb = pixelColor;:
            // TODO: VERIFY THIS WORKS or use: *crgb = pixelColor;:
            crgb[0] = pixelColor;
            nextStateGrid[y][j].state = GRID_STATE_FALLING;
            nextStateGrid[y][j].velocity = pixelVelocity + gravity;
            nextStateGrid[y][j].kValue = pixelKValue;
            moved = true;
            break;
          }
          else if ((belowStateA != NULL && belowStateA->state == GRID_STATE_NONE) && (belowNextStateA != NULL && belowNextStateA->state == GRID_STATE_NONE))
          {
            // This pixel will fall to side A (right)
            // leds[getPanelXYOffset(j + direction, y)] = pixelColor;
            CRGB *crgb = getCrgb(j + direction, y);
            // TODO: VERIFY THIS WORKS or use: *crgb = pixelColor;:
            // TODO: VERIFY THIS WORKS or use: *crgb = pixelColor;:
            // TODO: VERIFY THIS WORKS or use: *crgb = pixelColor;:
            crgb[0] = pixelColor;
            nextStateGrid[y][j + direction].state = GRID_STATE_FALLING;
            nextStateGrid[y][j + direction].velocity = pixelVelocity + gravity;
            nextStateGrid[y][j + direction].kValue = pixelKValue;
            moved = true;
            break;
          }
          else if ((belowStateB != NULL && belowStateB->state == GRID_STATE_NONE) && (belowNextStateB != NULL && belowNextStateB->state == GRID_STATE_NONE))
          {
            // This pixel will fall to side B (left)
            // leds[getPanelXYOffset(j - direction, y)] = pixelColor;
            CRGB *crgb = getCrgb(j - direction, y);
            // TODO: VERIFY THIS WORKS or use: *crgb = pixelColor;:
            // TODO: VERIFY THIS WORKS or use: *crgb = pixelColor;:
            // TODO: VERIFY THIS WORKS or use: *crgb = pixelColor;:
            crgb[0] = pixelColor;
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
        // setColor(&leds[getPanelXYOffset(j, i)], 0, 0, 0);
        setColor(getCrgb(j, i), 0, 0, 0);

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
