#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include <pgmspace.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#include <Adafruit_BMP280.h>
#include <TinyGPSPlus.h>

// =====================================================
// HARDWARE
// =====================================================

TFT_eSPI tft = TFT_eSPI();
Adafruit_BMP280 bmp;
TinyGPSPlus gps;

HardwareSerial gpsSerial(2);

#define SCREEN_W 240
#define SCREEN_H 240

#define BLACK 0x0000
#define WHITE 0xFFFF

// BMP280 I2C
#define I2C_SDA 21
#define I2C_SCL 22

// GPS TX -> ESP32 GPIO16
// GPS RX -> ESP32 GPIO17
#define GPS_RX 16
#define GPS_TX 17

#define BMP280_ADDR_1 0x76
#define BMP280_ADDR_2 0x77

const float SEALEVELPRESSURE_HPA = 1013.25f;

// =====================================================
// LIVE VALUES
// =====================================================

char temperature[12] = "--.-";
char altitude[16]    = "---.-m";
char clockTime[8]    = "--:--";
char amLabel[4]      = "--";

bool bmpAvailable = false;

unsigned long lastUpdate = 0;
const unsigned long UPDATE_INTERVAL = 1000;

// =====================================================
// LAYOUT
// =====================================================

const int TOP_ROW_Y = 87;

const int ICON_TEXT_GAP = 5;
const int THERMOMETER_W = 17;
const int MOUNTAIN_W = 29;

const int TEMPERATURE_SHIFT_X = 1;
const int ALTITUDE_SHIFT_X = -11;

// Move ONLY the mountain icon.
// The altitude text position will not change.
const int MOUNTAIN_SHIFT_X = 2;

const int CLOCK_CENTER_Y = 142;
const int AM_CENTER_Y = 148;
const int CLOCK_AM_GAP = 5;

const float CLOCK_SCALE = 1.4f;

// Fixed positions for the top row
int tempIconX = 0;
int tempFieldX = 0;
int tempFieldW = 0;
int tempDegreeX = 0;

int altIconX = 0;
int altFieldX = 0;
int altFieldW = 0;

// =====================================================
// THERMOMETER ICON
// Drawn once during setup
// =====================================================

void drawThermometer(int x, int centerY) {
  int topY = centerY - 10;

  tft.drawRoundRect(x + 6, topY, 7, 16, 3, WHITE);
  tft.drawLine(x + 9, topY + 5, x + 9,
               centerY + 3, WHITE);

  tft.drawCircle(x + 9, centerY + 7, 4, WHITE);
  tft.fillCircle(x + 9, centerY + 7, 2, WHITE);

  tft.drawFastHLine(x + 1, topY + 4, 4, WHITE);
  tft.drawFastHLine(x + 1, topY + 9, 4, WHITE);
  tft.drawFastHLine(x + 1, topY + 13, 4, WHITE);
}

// =====================================================
// MOUNTAIN ICON
// Drawn once at the shifted position
// =====================================================

void drawMountain(int x, int centerY) {
  tft.drawLine(x + 1, centerY + 9,
               x + 9, centerY - 4, WHITE);

  tft.drawLine(x + 9, centerY - 4,
               x + 17, centerY + 9, WHITE);

  tft.drawLine(x + 1, centerY + 9,
               x + 17, centerY + 9, WHITE);

  tft.drawLine(x + 11, centerY + 9,
               x + 20, centerY - 8, WHITE);

  tft.drawLine(x + 20, centerY - 8,
               x + 28, centerY + 9, WHITE);

  tft.drawLine(x + 11, centerY + 9,
               x + 28, centerY + 9, WHITE);
}

// =====================================================
// INITIALISE FIXED POSITIONS
// =====================================================

void initialiseLayout() {
  tft.setFreeFont(&FreeSans12pt7b);

  // Temperature field
  tempFieldW = tft.textWidth("-88.8");

  const int tempGroupW =
    THERMOMETER_W + ICON_TEXT_GAP
    + tempFieldW + 7;

  tempIconX =
    SCREEN_W / 4 - tempGroupW / 2
    + TEMPERATURE_SHIFT_X;

  tempFieldX =
    tempIconX + THERMOMETER_W + ICON_TEXT_GAP;

  tempDegreeX = tempFieldX + tempFieldW + 3;

  // Altitude field
  altFieldW = tft.textWidth("-888.8m");

  const int altGroupW =
    MOUNTAIN_W + ICON_TEXT_GAP + altFieldW;

  // IMPORTANT:
  // Keep this position independent of MOUNTAIN_SHIFT_X.
  // This ensures the altitude numbers never move.
  altIconX =
    (SCREEN_W * 3) / 4 - altGroupW / 2
    + ALTITUDE_SHIFT_X;

  altFieldX =
    altIconX + MOUNTAIN_W + ICON_TEXT_GAP;
}

// =====================================================
// TEMPERATURE VALUE
// Clears and updates only the temperature field
// =====================================================

void drawTemperatureValue() {
  tft.setFreeFont(&FreeSans12pt7b);
  tft.setTextColor(WHITE, BLACK);
  tft.setTextDatum(MC_DATUM);

  tft.fillRect(
    tempFieldX - 2,
    TOP_ROW_Y - 20,
    tempFieldW + 9,
    40,
    BLACK
  );

  tft.drawString(
    temperature,
    tempFieldX + tempFieldW / 2,
    TOP_ROW_Y
  );

  tft.drawCircle(
    tempDegreeX,
    TOP_ROW_Y - 7,
    2,
    WHITE
  );
}

// =====================================================
// ALTITUDE VALUE
// Clears ONLY the text field.
// Never draws or clears the mountain icon.
// =====================================================

void drawAltitudeValue() {
  tft.setFreeFont(&FreeSans12pt7b);
  tft.setTextColor(WHITE, BLACK);
  tft.setTextDatum(MC_DATUM);

  // Start clearing after the shifted mountain icon.
  // The mountain's rightmost coordinate is
  // altIconX + MOUNTAIN_SHIFT_X + 28.
  // This clearing region starts at altIconX + 32.
  tft.fillRect(
    altFieldX - 2,
    TOP_ROW_Y - 20,
    altFieldW + 4,
    40,
    BLACK
  );

  // Do not call drawMountain() here.
  // The icon is already drawn during setup.

  tft.drawString(
    altitude,
    altFieldX + altFieldW / 2,
    TOP_ROW_Y
  );
}

// =====================================================
// UPDATE HELPERS
// =====================================================

void setTemperature(const char* newValue) {
  if (strcmp(temperature, newValue) == 0) {
    return;
  }

  strncpy(temperature, newValue, sizeof(temperature) - 1);
  temperature[sizeof(temperature) - 1] = '\0';

  drawTemperatureValue();
}

void setAltitude(const char* newValue) {
  if (strcmp(altitude, newValue) == 0) {
    return;
  }

  strncpy(altitude, newValue, sizeof(altitude) - 1);
  altitude[sizeof(altitude) - 1] = '\0';

  drawAltitudeValue();
}

// =====================================================
// FONT GLYPH HELPER
// =====================================================

bool readGlyph(
  const GFXfont* sourceFont,
  GFXfont& fontData,
  char character,
  GFXglyph& glyph
) {
  memcpy_P(&fontData, sourceFont, sizeof(GFXfont));

  uint16_t characterCode = (uint8_t)character;

  if (characterCode < fontData.first ||
      characterCode > fontData.last) {
    return false;
  }

  memcpy_P(
    &glyph,
    &fontData.glyph[characterCode - fontData.first],
    sizeof(GFXglyph)
  );

  return true;
}

// =====================================================
// MEASURE SCALED TEXT
// =====================================================

int measureScaledTextWidth(
  const char* text,
  const GFXfont* sourceFont,
  float scale
) {
  GFXfont fontData;
  memcpy_P(&fontData, sourceFont, sizeof(GFXfont));

  float totalAdvance = 0.0f;

  for (int i = 0; text[i] != '\0'; i++) {
    GFXglyph glyph;

    if (readGlyph(sourceFont, fontData, text[i], glyph)) {
      totalAdvance += glyph.xAdvance;
    }
  }

  return (int)lroundf(totalAdvance * scale);
}

// =====================================================
// DRAW SCALED FREEFONT TEXT
// =====================================================

void drawScaledFreeText(
  const char* text,
  const GFXfont* sourceFont,
  float scale,
  int centerX,
  int centerY
) {
  GFXfont fontData;
  memcpy_P(&fontData, sourceFont, sizeof(GFXfont));

  int minTop = 127;
  int maxBottom = -128;

  for (int i = 0; text[i] != '\0'; i++) {
    GFXglyph glyph;

    if (!readGlyph(sourceFont, fontData, text[i], glyph)) {
      continue;
    }

    if (glyph.height > 0) {
      if (glyph.yOffset < minTop) {
        minTop = glyph.yOffset;
      }

      int glyphBottom = glyph.yOffset + glyph.height;

      if (glyphBottom > maxBottom) {
        maxBottom = glyphBottom;
      }
    }
  }

  if (maxBottom < minTop) {
    return;
  }

  int width = measureScaledTextWidth(
    text, sourceFont, scale
  );

  int startX = centerX - width / 2;

  int scaledTop = (int)floorf(minTop * scale);
  int scaledBottom = (int)ceilf(maxBottom * scale);

  int verticalOffset =
    (int)lroundf((scaledTop + scaledBottom) / 2.0f);

  int baselineY = centerY - verticalOffset;

  float cursorAdvance = 0.0f;

  for (int i = 0; text[i] != '\0'; i++) {
    GFXglyph glyph;

    if (!readGlyph(sourceFont, fontData, text[i], glyph)) {
      continue;
    }

    for (int yy = 0; yy < glyph.height; yy++) {
      for (int xx = 0; xx < glyph.width; xx++) {
        uint32_t bitIndex =
          (uint32_t)yy * glyph.width + xx;

        uint8_t bitmapByte = pgm_read_byte(
          fontData.bitmap
          + glyph.bitmapOffset
          + (bitIndex >> 3)
        );

        if (bitmapByte & (0x80 >> (bitIndex & 7))) {
          float logicalX =
            cursorAdvance + glyph.xOffset * scale
            + xx * scale;

          float logicalXNext =
            cursorAdvance + glyph.xOffset * scale
            + (xx + 1) * scale;

          float logicalY =
            (glyph.yOffset + yy) * scale;

          float logicalYNext =
            (glyph.yOffset + yy + 1) * scale;

          int pixelX =
            startX + (int)lroundf(logicalX);

          int pixelY =
            baselineY + (int)lroundf(logicalY);

          int pixelW =
            (int)lroundf(logicalXNext)
            - (int)lroundf(logicalX);

          int pixelH =
            (int)lroundf(logicalYNext)
            - (int)lroundf(logicalY);

          if (pixelW < 1) pixelW = 1;
          if (pixelH < 1) pixelH = 1;

          tft.fillRect(
            pixelX, pixelY, pixelW, pixelH, WHITE
          );
        }
      }
    }

    cursorAdvance += glyph.xAdvance * scale;
  }
}

// =====================================================
// CLOCK
// =====================================================

void drawClock() {
  // Clear only the clock area.
  tft.fillRect(0, 104, SCREEN_W, 80, BLACK);

  int clockW = measureScaledTextWidth(
    clockTime,
    &FreeSans24pt7b,
    CLOCK_SCALE
  );

  tft.setFreeFont(&FreeSans12pt7b);
  int amW = tft.textWidth(amLabel);

  int totalW = clockW + CLOCK_AM_GAP + amW;
  int startX = (SCREEN_W - totalW) / 2;

  int clockCentreX = startX + clockW / 2;

  int amCentreX =
    startX + clockW + CLOCK_AM_GAP + amW / 2;

  drawScaledFreeText(
    clockTime,
    &FreeSans24pt7b,
    CLOCK_SCALE,
    clockCentreX,
    CLOCK_CENTER_Y
  );

  tft.setFreeFont(&FreeSans12pt7b);
  tft.setTextColor(WHITE, BLACK);
  tft.setTextDatum(MC_DATUM);

  tft.drawString(
    amLabel,
    amCentreX,
    AM_CENTER_Y
  );
}

// =====================================================
// BMP280 UPDATE
// =====================================================

void updateBMP280() {
  if (!bmpAvailable) {
    setTemperature("--.-");
    setAltitude("---.-m");
    return;
  }

  float tempC = bmp.readTemperature();
  float pressurePa = bmp.readPressure();

  if (!isfinite(tempC) ||
      !isfinite(pressurePa) ||
      pressurePa <= 0.0f) {
    setTemperature("--.-");
    setAltitude("---.-m");
    return;
  }

  char newTemperature[12];
  char newAltitude[16];

  snprintf(
    newTemperature,
    sizeof(newTemperature),
    "%.1f",
    tempC
  );

  float altitudeM =
    bmp.readAltitude(SEALEVELPRESSURE_HPA);

  if (isfinite(altitudeM)) {
    snprintf(
      newAltitude,
      sizeof(newAltitude),
      "%.1fm",
      altitudeM
    );
  } else {
    strcpy(newAltitude, "---.-m");
  }

  setTemperature(newTemperature);
  setAltitude(newAltitude);
}

// =====================================================
// GPS TIME UPDATE
// GPS UTC -> Singapore time (UTC+8)
// =====================================================

void updateGPSTime() {
  char newTime[8];
  char newAmLabel[4];

if (!gps.time.isValid() ||
    gps.time.age() >= 2500 ||
    !gps.date.isValid() ||
    gps.date.age() >= 2500 ||
    !gps.location.isValid() ||
    gps.location.age() >= 5000) {

  strcpy(newTime, "--:--");
  strcpy(newAmLabel, "--");
  } else {
    int utcHour = gps.time.hour();
    int minute = gps.time.minute();

    int singaporeHour = (utcHour + 8) % 24;

    int hour12 = singaporeHour % 12;

    if (hour12 == 0) {
      hour12 = 12;
    }

    snprintf(
      newTime,
      sizeof(newTime),
      "%02d:%02d",
      hour12,
      minute
    );

    if (singaporeHour < 12) {
      strcpy(newAmLabel, "am");
    } else {
      strcpy(newAmLabel, "pm");
    }
  }

  // Refresh only when the visible time changes.
  if (strcmp(clockTime, newTime) != 0 ||
      strcmp(amLabel, newAmLabel) != 0) {

    strcpy(clockTime, newTime);
    strcpy(amLabel, newAmLabel);

    drawClock();
  }
}

// =====================================================
// PROCESS GPS DATA
// =====================================================

void processGPS() {
  while (gpsSerial.available() > 0) {
    char c = (char)gpsSerial.read();

    gps.encode(c);

    // Forward raw GPS NMEA data to Serial Monitor.
    Serial.write(c);
  }
}

// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(115200);

  Wire.begin(I2C_SDA, I2C_SCL);

  gpsSerial.begin(
    9600,
    SERIAL_8N1,
    GPS_RX,
    GPS_TX
  );

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(BLACK);

  // Detect BMP280.
  bmpAvailable = bmp.begin(BMP280_ADDR_1);

  if (!bmpAvailable) {
    bmpAvailable = bmp.begin(BMP280_ADDR_2);
  }

  if (bmpAvailable) {
    bmp.setSampling(
      Adafruit_BMP280::MODE_NORMAL,
      Adafruit_BMP280::SAMPLING_X2,
      Adafruit_BMP280::SAMPLING_X16,
      Adafruit_BMP280::FILTER_X16,
      Adafruit_BMP280::STANDBY_MS_500
    );

    Serial.println("\nBMP280 detected.");
  } else {
    Serial.println("\nERROR: BMP280 not detected.");
  }

  Serial.println("GPS UART started at 9600 baud.");
  Serial.println("Receiving GPS NMEA data...");

  initialiseLayout();

  // Draw each static icon exactly once.
  drawThermometer(tempIconX, TOP_ROW_Y);

  // The +2 shift applies ONLY to the mountain icon.
  drawMountain(
    altIconX + 3,
    TOP_ROW_Y
  );

  // Draw initial values.
  drawTemperatureValue();
  drawAltitudeValue();
  drawClock();

  lastUpdate = millis();
}

// =====================================================
// MAIN LOOP
// =====================================================

void loop() {
  // Continuously consume GPS input.
  processGPS();

  unsigned long now = millis();

  if (now - lastUpdate >= UPDATE_INTERVAL) {
    lastUpdate = now;

    updateBMP280();
    updateGPSTime();
  }
}
