
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include <TinyGPSPlus.h>
#include <math.h>

// =====================================================
// DISPLAY
// =====================================================

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);

#define SCREEN_W 240
#define SCREEN_H 240
#define CX 120
#define CY 120

// =====================================================
// GPS: NEO-6M
// =====================================================

HardwareSerial gpsSerial(2);
TinyGPSPlus gps;

#define GPS_RX_PIN 16
#define GPS_TX_PIN 17

bool gpsFix = false;
bool gpsSpeedValid = false;
bool gpsNmeaValid = false;

float speedKmh = 0.0f;

unsigned long lastValidNmeaTime = 0;

const unsigned long GPS_NMEA_TIMEOUT = 3000UL;

// Buffer for validating complete NMEA sentences.
char nmeaBuffer[128];
size_t nmeaIndex = 0;

// =====================================================
// COMPASS: HMC5883L
// =====================================================

#define HMC_SDA_PIN 21
#define HMC_SCL_PIN 22
#define HMC_ADDR 0x1E

const float MAG_OFFSET_X = 0.0f;
const float MAG_OFFSET_Y = 0.0f;

const float MAG_DECLINATION_DEG = 0.0f;

bool hmcOk = false;
bool compassValid = false;

float heading = 0.0f;
unsigned long lastCompassRead = 0;

// =====================================================
// SPEED SCALE
// =====================================================

const float SPEED_MIN = 0.0f;
const float SPEED_MAX = 60.0f;

// =====================================================
// COLOURS (RGB565)
// =====================================================

#define BLACK       0x0000
#define WHITE       0xFFFF
#define GREEN       0x07E0
#define RED         0xF800
#define YELLOW      0xFFE0
#define LIGHT_GREY  0xC618
#define DARK_GREY   0x5AEB

const int OUTER_CIRCLE_RADIUS = 119;

// =====================================================
// SPEEDOMETER LAYOUT
// =====================================================

const int SPEED_CX = 120;
const int SPEED_CY = 160;

const int SPEED_CIRCLE_RADIUS = 79;

const int ARC_INNER = 62;
const int ARC_OUTER = 71;

const int NEEDLE_LENGTH = 58;

const float ARC_START_ANGLE = -114.0f;
const float ARC_END_ANGLE   =  114.0f;

// =====================================================
// NMEA SENTENCE VALIDATION
// =====================================================
// Validates the checksum of a complete NMEA sentence.
// A valid sentence starts with '$' and contains '*HH',
// where HH is the hexadecimal checksum.
// =====================================================

bool isValidNmeaSentence(const char *sentence)
{
  if (sentence == nullptr || sentence[0] != '$')
    return false;

  const char *star = strchr(sentence, '*');

  if (star == nullptr)
    return false;

  // Exactly two checksum characters must follow '*'.
  if (star[1] == '\0' || star[2] == '\0')
    return false;

  if (star[3] != '\0')
    return false;

  uint8_t calculatedChecksum = 0;

  // XOR all characters between '$' and '*'.
  for (const char *p = sentence + 1; p < star; p++)
  {
    calculatedChecksum ^= (uint8_t)*p;
  }

  char *endPointer = nullptr;

  unsigned long receivedChecksum =
    strtoul(star + 1, &endPointer, 16);

  if (endPointer == star + 1 || *endPointer != '\0')
    return false;

  if (receivedChecksum > 0xFF)
    return false;

  return calculatedChecksum == (uint8_t)receivedChecksum;
}

// =====================================================
// HMC5883L I2C FUNCTIONS
// =====================================================

bool writeHMCRegister(uint8_t reg, uint8_t value)
{
  Wire.beginTransmission(HMC_ADDR);
  Wire.write(reg);
  Wire.write(value);

  return Wire.endTransmission() == 0;
}

bool initHMC5883L()
{
  // 8-sample averaging, 15 Hz output rate.
  if (!writeHMCRegister(0x00, 0x70))
    return false;

  // Gain = 1.3 Ga.
  if (!writeHMCRegister(0x01, 0x20))
    return false;

  // Continuous measurement mode.
  if (!writeHMCRegister(0x02, 0x00))
    return false;

  delay(10);

  return true;
}

bool readHMC5883L(int16_t &x, int16_t &y, int16_t &z)
{
  Wire.beginTransmission(HMC_ADDR);
  Wire.write(0x03);

  if (Wire.endTransmission(false) != 0)
    return false;

  uint8_t count =
    Wire.requestFrom((uint8_t)HMC_ADDR, (uint8_t)6);

  if (count != 6)
  {
    while (Wire.available())
      Wire.read();

    return false;
  }

  // HMC5883L register order: X, Z, Y.
  uint8_t xMsb = Wire.read();
  uint8_t xLsb = Wire.read();

  uint8_t zMsb = Wire.read();
  uint8_t zLsb = Wire.read();

  uint8_t yMsb = Wire.read();
  uint8_t yLsb = Wire.read();

  x = (int16_t)(((uint16_t)xMsb << 8) | xLsb);
  z = (int16_t)(((uint16_t)zMsb << 8) | zLsb);
  y = (int16_t)(((uint16_t)yMsb << 8) | yLsb);

  return true;
}

void updateCompass()
{
  if (!hmcOk)
  {
    compassValid = false;
    return;
  }

  // Update approximately every 70 ms.
  if (millis() - lastCompassRead < 70)
    return;

  lastCompassRead = millis();

  int16_t rawX, rawY, rawZ;

  if (!readHMC5883L(rawX, rawY, rawZ))
  {
    compassValid = false;
    return;
  }

  // Sensor overflow check.
  if (rawX == -4096 ||
      rawY == -4096 ||
      rawZ == -4096)
  {
    compassValid = false;
    return;
  }

  float x = (float)rawX - MAG_OFFSET_X;
  float y = (float)rawY - MAG_OFFSET_Y;

  if (fabsf(x) < 0.5f && fabsf(y) < 0.5f)
  {
    compassValid = false;
    return;
  }

  float angle = atan2f(y, x) * 180.0f / PI;

  angle += MAG_DECLINATION_DEG;

  while (angle < 0.0f)
    angle += 360.0f;

  while (angle >= 360.0f)
    angle -= 360.0f;

  heading = angle;
  compassValid = true;
}

// =====================================================
// GPS UPDATE + RAW NMEA SERIAL MONITOR OUTPUT
// =====================================================

void updateGPS()
{
  // Read all available GPS characters.
  while (gpsSerial.available() > 0)
  {
    char c = (char)gpsSerial.read();

    // Forward the original GPS stream to the PC.
    // Open Serial Monitor at 115200 baud.
    Serial.write((uint8_t)c);

    // Feed TinyGPSPlus for location and speed parsing.
    gps.encode(c);

    // Build a complete NMEA sentence for checksum
    // validation. GPS sentences end with '\n'.
    if (c == '\n')
    {
      nmeaBuffer[nmeaIndex] = '\0';

      // Remove a trailing carriage return if present.
      if (nmeaIndex > 0 &&
          nmeaBuffer[nmeaIndex - 1] == '\r')
      {
        nmeaBuffer[nmeaIndex - 1] = '\0';
      }

      if (isValidNmeaSentence(nmeaBuffer))
      {
        lastValidNmeaTime = millis();
      }

      nmeaIndex = 0;
    }
    else if (c != '\r')
    {
      if (nmeaIndex < sizeof(nmeaBuffer) - 1)
      {
        nmeaBuffer[nmeaIndex++] = c;
      }
      else
      {
        // Discard an overlong sentence and restart.
        nmeaIndex = 0;
      }
    }
  }

  // NMEA communication must be recent.
  gpsNmeaValid =
    lastValidNmeaTime != 0 &&
    millis() - lastValidNmeaTime < GPS_NMEA_TIMEOUT;

  // GPS position fix must be valid and recent.
  gpsFix =
    gpsNmeaValid &&
    gps.location.isValid() &&
    gps.location.age() < 3000UL;

  // Speed is usable only with a recent position fix.
  gpsSpeedValid =
    gpsFix &&
    gps.speed.isValid() &&
    gps.speed.age() < 3000UL;

  if (gpsSpeedValid)
  {
    float receivedSpeed = gps.speed.kmph();

    if (isfinite(receivedSpeed) && receivedSpeed >= 0.0f)
    {
      speedKmh = constrain(
        receivedSpeed,
        SPEED_MIN,
        SPEED_MAX
      );
    }
    else
    {
      gpsSpeedValid = false;
      speedKmh = 0.0f;
    }
  }
  else
  {
    speedKmh = 0.0f;
  }
}

// =====================================================
// ANGLE FUNCTIONS
// =====================================================

float degToRad(float deg)
{
  return deg * PI / 180.0f;
}

int angleX(float angle, float radius, int cx)
{
  return cx + (int)roundf(
    sinf(degToRad(angle)) * radius
  );
}

int angleY(float angle, float radius, int cy)
{
  return cy - (int)roundf(
    cosf(degToRad(angle)) * radius
  );
}

// =====================================================
// SPEED TO ANGLE
// =====================================================

float speedToAngle(float speed)
{
  speed = constrain(speed, SPEED_MIN, SPEED_MAX);

  float percentage =
    (speed - SPEED_MIN) /
    (SPEED_MAX - SPEED_MIN);

  return ARC_START_ANGLE +
         percentage *
         (ARC_END_ANGLE - ARC_START_ANGLE);
}

// =====================================================
// CARDINAL DIRECTION
// =====================================================

String getCardinal(float h)
{
  if (h >= 337.5f || h < 22.5f)
    return "N";

  if (h < 67.5f)
    return "NE";

  if (h < 112.5f)
    return "E";

  if (h < 157.5f)
    return "SE";

  if (h < 202.5f)
    return "S";

  if (h < 247.5f)
    return "SW";

  if (h < 292.5f)
    return "W";

  return "NW";
}

// =====================================================
// SMOOTH GREEN -> YELLOW -> RED GRADIENT
// =====================================================

uint16_t gradientColour(float amount)
{
  amount = constrain(amount, 0.0f, 1.0f);

  uint8_t r;
  uint8_t g;
  uint8_t b = 0;

  if (amount < 0.5f)
  {
    // Green -> yellow.
    float t = amount * 2.0f;
    t = t * t * (3.0f - 2.0f * t);

    r = (uint8_t)roundf(255.0f * t);
    g = 255;
  }
  else
  {
    // Yellow -> red.
    float t = (amount - 0.5f) * 2.0f;
    t = t * t * (3.0f - 2.0f * t);

    r = 255;
    g = (uint8_t)roundf(255.0f * (1.0f - t));
  }

  return (
    ((r & 0xF8) << 8) |
    ((g & 0xFC) << 3) |
    (b >> 3)
  );
}

// =====================================================
// DRAW ARC SECTION
// =====================================================

void drawArcSection(
  float startAngle,
  float endAngle,
  uint16_t colour
)
{
  const float step = 1.0f;

  for (float a = startAngle; a < endAngle; a += step)
  {
    float a1 = a;
    float a2 = a + step;

    if (a2 > endAngle)
      a2 = endAngle;

    // Overlap slightly to avoid gaps.
    a1 -= 0.15f;
    a2 += 0.15f;

    if (a1 < startAngle)
      a1 = startAngle;

    if (a2 > endAngle)
      a2 = endAngle;

    int x1o = angleX(a1, ARC_OUTER, SPEED_CX);
    int y1o = angleY(a1, ARC_OUTER, SPEED_CY);

    int x2o = angleX(a2, ARC_OUTER, SPEED_CX);
    int y2o = angleY(a2, ARC_OUTER, SPEED_CY);

    int x1i = angleX(a1, ARC_INNER, SPEED_CX);
    int y1i = angleY(a1, ARC_INNER, SPEED_CY);

    int x2i = angleX(a2, ARC_INNER, SPEED_CX);
    int y2i = angleY(a2, ARC_INNER, SPEED_CY);

    sprite.fillTriangle(
      x1o, y1o,
      x2o, y2o,
      x1i, y1i,
      colour
    );

    sprite.fillTriangle(
      x2o, y2o,
      x2i, y2i,
      x1i, y1i,
      colour
    );
  }
}

// =====================================================
// FULL GREY SPEED TRACK
// =====================================================

void drawSpeedTrack()
{
  drawArcSection(
    ARC_START_ANGLE,
    ARC_END_ANGLE,
    DARK_GREY
  );
}

// =====================================================
// GRADIENT REVEALED UP TO THE NEEDLE
// =====================================================

void drawSpeedGradient(float speed)
{
  speed = constrain(speed, SPEED_MIN, SPEED_MAX);

  float currentAngle = speedToAngle(speed);

  const float step = 0.5f;

  for (
    float a = ARC_START_ANGLE;
    a < currentAngle;
    a += step
  )
  {
    float a1 = a;
    float a2 = a + step;

    if (a2 > currentAngle)
      a2 = currentAngle;

    float midpoint = (a1 + a2) * 0.5f;

    float gradientPosition =
      (midpoint - ARC_START_ANGLE) /
      (ARC_END_ANGLE - ARC_START_ANGLE);

    uint16_t colour = gradientColour(gradientPosition);

    // Small overlap prevents gaps.
    a1 -= 0.10f;
    a2 += 0.10f;

    if (a1 < ARC_START_ANGLE)
      a1 = ARC_START_ANGLE;

    if (a2 > currentAngle)
      a2 = currentAngle;

    int x1o = angleX(a1, ARC_OUTER, SPEED_CX);
    int y1o = angleY(a1, ARC_OUTER, SPEED_CY);

    int x2o = angleX(a2, ARC_OUTER, SPEED_CX);
    int y2o = angleY(a2, ARC_OUTER, SPEED_CY);

    int x1i = angleX(a1, ARC_INNER, SPEED_CX);
    int y1i = angleY(a1, ARC_INNER, SPEED_CY);

    int x2i = angleX(a2, ARC_INNER, SPEED_CX);
    int y2i = angleY(a2, ARC_INNER, SPEED_CY);

    sprite.fillTriangle(
      x1o, y1o,
      x2o, y2o,
      x1i, y1i,
      colour
    );

    sprite.fillTriangle(
      x2o, y2o,
      x2i, y2i,
      x1i, y1i,
      colour
    );
  }
}

// =====================================================
// SPEED NEEDLE
// =====================================================

void drawSpeedNeedle(float speed)
{
  speed = constrain(speed, SPEED_MIN, SPEED_MAX);

  float angle = speedToAngle(speed);
  float rad = degToRad(angle);

  const float tipRadius = NEEDLE_LENGTH;
  const float baseHalfWidth = 4.0f;

  int tipX = SPEED_CX +
    (int)roundf(sinf(rad) * tipRadius);

  int tipY = SPEED_CY -
    (int)roundf(cosf(rad) * tipRadius);

  float perpRad = rad + PI / 2.0f;

  int leftX = SPEED_CX +
    (int)roundf(sinf(perpRad) * baseHalfWidth);

  int leftY = SPEED_CY -
    (int)roundf(cosf(perpRad) * baseHalfWidth);

  int rightX = SPEED_CX -
    (int)roundf(sinf(perpRad) * baseHalfWidth);

  int rightY = SPEED_CY +
    (int)roundf(cosf(perpRad) * baseHalfWidth);

  // Needle body.
  sprite.fillTriangle(
    leftX, leftY,
    rightX, rightY,
    tipX, tipY,
    LIGHT_GREY
  );

  // Thin centre line.
  int lineEndX = SPEED_CX +
    (int)roundf(sinf(rad) * (tipRadius - 1.0f));

  int lineEndY = SPEED_CY -
    (int)roundf(cosf(rad) * (tipRadius - 1.0f));

  sprite.drawLine(
    SPEED_CX, SPEED_CY,
    lineEndX, lineEndY,
    LIGHT_GREY
  );

  // Small pivot circle.
  sprite.fillCircle(
    SPEED_CX,
    SPEED_CY,
    4,
    LIGHT_GREY
  );
}

// =====================================================
// HEADING LADDER
// =====================================================

void drawHeadingLadder(float currentHeading)
{
  for (int bearing = 0; bearing < 360; bearing += 15)
  {
    float relative = (float)bearing - currentHeading;

    while (relative > 180.0f)
      relative -= 360.0f;

    while (relative < -180.0f)
      relative += 360.0f;

    if (relative < -82.0f || relative > 82.0f)
      continue;

    int tickLength = (bearing % 30 == 0) ? 11 : 8;

    float outerRadius = 113.0f;
    float innerRadius = outerRadius - tickLength;

    int x1 = angleX(relative, outerRadius, CX);
    int y1 = angleY(relative, outerRadius, CY);

    int x2 = angleX(relative, innerRadius, CX);
    int y2 = angleY(relative, innerRadius, CY);

    sprite.drawLine(x1, y1, x2, y2, WHITE);
  }

  // Fixed heading marker.
  sprite.fillTriangle(
    120, 27,
    113, 42,
    127, 42,
    WHITE
  );
}

// =====================================================
// HEADING TEXT
// =====================================================

void drawHeadingText(float currentHeading)
{
  sprite.setTextDatum(MC_DATUM);
  sprite.setTextColor(WHITE, BLACK);

  if (!compassValid)
  {
    sprite.drawString(
      hmcOk ? "MAG WAIT" : "MAG ERR",
      120, 61, 2
    );

    return;
  }

  String direction = getCardinal(currentHeading);

  String headingText =
    direction + " " + String(currentHeading, 1);

  sprite.drawString(headingText, 120, 61, 2);

  // Degree marker positioned relative to text width.
  int degreeX =
    120 + sprite.textWidth(headingText, 2) / 2 + 3;

  sprite.drawCircle(degreeX, 56, 2, WHITE);
}

// =====================================================
// GPS LOCATION PIN ICON
// =====================================================
// Green  = GPS fix acquired.
// Yellow = Valid NMEA data, no position fix.
// Red    = No recent valid NMEA data.
// =====================================================

void drawGpsPin(
  int cx,
  int cy,
  bool hasGpsFix,
  bool hasValidNmea
)
{
  uint16_t pinColour;

  if (hasGpsFix)
  {
    pinColour = GREEN;
  }
  else if (hasValidNmea)
  {
    pinColour = YELLOW;
  }
  else
  {
    pinColour = RED;
  }

  // Pointed bottom.
  sprite.fillTriangle(
    cx - 5, cy,
    cx + 5, cy,
    cx,     cy + 9,
    pinColour
  );

  // Rounded upper body.
  sprite.fillCircle(
    cx, cy - 2,
    6,
    pinColour
  );

  // Inner location circle.
  sprite.fillCircle(
    cx, cy - 2,
    2,
    BLACK
  );
}

// =====================================================
// SPEED TEXT AND GPS PIN POSITION
// =====================================================

void drawSpeedText(
  float speed,
  bool hasValidSpeed,
  bool hasGpsFix
)
{
  String numberText =
    hasValidSpeed ? String(speed, 1) : "--.-";

  String unitText = "km/h";

  sprite.setTextDatum(MC_DATUM);
  sprite.setTextColor(WHITE, BLACK);

  // Large speed number.
  sprite.drawString(
    numberText,
    SPEED_CX,
    195,
    4
  );

  // Small unit label.
  sprite.drawString(
    unitText,
    SPEED_CX,
    216,
    2
  );

  // GPS pin: between the number and unit,
  // shifted slightly upwards and to the right.
  int pinX = SPEED_CX + 37;
  int pinY = 207;

  drawGpsPin(
    pinX,
    pinY,
    hasGpsFix,
    gpsNmeaValid
  );
}

// =====================================================
// COMPLETE UI
// =====================================================

void drawUI()
{
  sprite.fillSprite(BLACK);

  // Heading information from the HMC5883L.
  if (compassValid)
    drawHeadingLadder(heading);

  drawHeadingText(heading);

  // Speedometer outline.
  sprite.drawCircle(
    SPEED_CX,
    SPEED_CY,
    SPEED_CIRCLE_RADIUS,
    WHITE
  );

  // Grey baseline.
  drawSpeedTrack();

  // Reveal gradient only with valid GPS speed.
  if (gpsSpeedValid)
    drawSpeedGradient(speedKmh);

  // Needle stays at zero when speed is unavailable.
  drawSpeedNeedle(speedKmh);

  // Speed and GPS status pin.
  drawSpeedText(
    speedKmh,
    gpsSpeedValid,
    gpsFix
  );

  // Outer display circle.
  sprite.drawCircle(
    CX,
    CY,
    OUTER_CIRCLE_RADIUS,
    WHITE
  );

  sprite.pushSprite(0, 0);
}

// =====================================================
// SETUP
// =====================================================

void setup()
{
  // PC Serial Monitor.
  Serial.begin(115200);

  // GPS UART at 9600 baud.
  gpsSerial.begin(
    9600,
    SERIAL_8N1,
    GPS_RX_PIN,
    GPS_TX_PIN
  );

  // Initialise I2C and HMC5883L.
  Wire.begin(
    HMC_SDA_PIN,
    HMC_SCL_PIN
  );

  hmcOk = initHMC5883L();

  // These startup messages appear before the raw NMEA
  // stream. After this, incoming GPS characters are
  // forwarded directly to the Serial Monitor.
  Serial.println();
  Serial.println("ESP32 GPS + Compass Dashboard");
  Serial.println(
    hmcOk
      ? "HMC5883L detected"
      : "ERROR: HMC5883L not detected at 0x1E"
  );
  Serial.println("Raw GPS NMEA stream follows:");

  // Initialise display.
  tft.init();
  tft.setRotation(0);

  sprite.setColorDepth(8);

  sprite.createSprite(
    SCREEN_W,
    SCREEN_H
  );

  tft.fillScreen(BLACK);

  drawUI();
}

// =====================================================
// MAIN LOOP
// =====================================================

void loop()
{
  // Read GPS and forward raw NMEA to the PC.
  updateGPS();

  // Read compass.
  updateCompass();

  // Refresh display.
  drawUI();

  delay(30);
}
