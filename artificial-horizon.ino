#include <Wire.h>
#include <TFT_eSPI.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_HMC5883_U.h>

Adafruit_BMP280 bmp;
bool bmpOK = false;
float altitude = 0.0f;

// ============================================================
// HMC5883L
// ============================================================

Adafruit_HMC5883_Unified mag =
  Adafruit_HMC5883_Unified(12345);

bool magOK = false;
float heading = 0.0f;

// ============================================================
// TFT
// ============================================================

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite horizonSprite = TFT_eSprite(&tft);

// ============================================================
// SCREEN
// ============================================================

#define SCREEN_W 240
#define SCREEN_H 240

#define CX 120
#define CY 120

#define PFD_RADIUS 120
#define BORDER_RADIUS 119

// ============================================================
// COLORS
// ============================================================

#define SKY     0x65BF
#define GROUND  0x8B26

#define WHITE   0xFFFF
#define BLACK   0x0000
#define YELLOW  0xFFE0

// ============================================================
// MPU6050
// ============================================================

#define MPU_ADDR 0x68

// ============================================================
// ATTITUDE
// ============================================================

float roll = 0.0f;
float pitch = 0.0f;

float gyroBiasX = 0.0f;
float gyroBiasY = 0.0f;
float gyroBiasZ = 0.0f;

float levelRollOffset = 0.0f;
float levelPitchOffset = 0.0f;

unsigned long lastMicros = 0;
unsigned long lastSerial = 0;





//loading
void drawLoadingScreen()
{
  tft.fillScreen(BLACK);

  tft.setTextDatum(MC_DATUM);

  tft.setTextColor(WHITE, BLACK);
  tft.setTextSize(2);

  tft.drawString(
    "All sensors OK",
    CX,
    CY - 12
  );

  tft.setTextSize(2);

  tft.drawString(
    "Calibrating...",
    CX,
    CY + 15
  );

  tft.setTextDatum(TL_DATUM);
}



// ============================================================
// MPU WRITE
// ============================================================

void writeMPU(uint8_t reg, uint8_t value)
{
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

// ============================================================
// MPU READ
// ============================================================

bool readMPU(
  int16_t &ax,
  int16_t &ay,
  int16_t &az,
  int16_t &gx,
  int16_t &gy,
  int16_t &gz
)
{
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);

  if (Wire.endTransmission(false) != 0)
    return false;

  uint8_t count =
    Wire.requestFrom(
      MPU_ADDR,
      (uint8_t)14,
      (uint8_t)true
    );

  if (count != 14 || Wire.available() < 14)
    return false;

  ax = (int16_t)((Wire.read() << 8) | Wire.read());
  ay = (int16_t)((Wire.read() << 8) | Wire.read());
  az = (int16_t)((Wire.read() << 8) | Wire.read());

  // Temperature
  Wire.read();
  Wire.read();

  gx = (int16_t)((Wire.read() << 8) | Wire.read());
  gy = (int16_t)((Wire.read() << 8) | Wire.read());
  gz = (int16_t)((Wire.read() << 8) | Wire.read());

  return true;
}

// ============================================================
// MPU SETUP
// ============================================================

void setupMPU()
{
  // Wake MPU6050
  writeMPU(0x6B, 0x00);

  delay(100);

  // Accelerometer ±2g
  writeMPU(0x1C, 0x00);

  // Gyroscope ±250°/s
  writeMPU(0x1B, 0x00);

  // Digital low-pass filter
  writeMPU(0x1A, 0x03);

  // Sample divider
  writeMPU(0x19, 0x04);
}

// ============================================================
// GYRO CALIBRATION
// ============================================================

void calibrateGyro()
{
  Serial.println();
  Serial.println("==============================");
  Serial.println("KEEP MPU6050 STILL");
  Serial.println("Calibrating gyro...");
  Serial.println("==============================");

  delay(1500);

  const int samples = 500;

  float sumX = 0;
  float sumY = 0;
  float sumZ = 0;

  int validSamples = 0;

  for (int i = 0; i < samples; i++)
  {
    int16_t ax, ay, az;
    int16_t gx, gy, gz;

    if (readMPU(ax, ay, az, gx, gy, gz))
    {
      sumX += gx / 131.0f;
      sumY += gy / 131.0f;
      sumZ += gz / 131.0f;

      validSamples++;
    }

    delay(2);
  }

  if (validSamples > 0)
  {
    gyroBiasX = sumX / validSamples;
    gyroBiasY = sumY / validSamples;
    gyroBiasZ = sumZ / validSamples;
  }

  Serial.println("Gyro calibration complete.");

  Serial.print("GX bias: ");
  Serial.println(gyroBiasX, 3);

  Serial.print("GY bias: ");
  Serial.println(gyroBiasY, 3);

  Serial.print("GZ bias: ");
  Serial.println(gyroBiasZ, 3);

  Serial.println();
}

// ============================================================
// LEVEL CALIBRATION
// ============================================================

void calibrateLevel()
{
  Serial.println("Finding level position...");

  float rollSum = 0;
  float pitchSum = 0;

  int validSamples = 0;

  for (int i = 0; i < 100; i++)
  {
    int16_t ax, ay, az;
    int16_t gx, gy, gz;

    if (readMPU(ax, ay, az, gx, gy, gz))
    {
      float x = ax / 16384.0f;
      float y = ay / 16384.0f;
      float z = az / 16384.0f;

      float accelRoll =
        atan2(y, z) * 180.0f / PI;

      float accelPitch =
        atan2(
          -x,
          sqrt(y * y + z * z)
        ) * 180.0f / PI;

      rollSum += accelRoll;
      pitchSum += accelPitch;

      validSamples++;
    }

    delay(5);
  }

  if (validSamples > 0)
  {
    levelRollOffset =
      rollSum / validSamples;

    levelPitchOffset =
      pitchSum / validSamples;
  }

  roll = 0;
  pitch = 0;

  Serial.print("Roll level offset: ");
  Serial.println(levelRollOffset, 2);

  Serial.print("Pitch level offset: ");
  Serial.println(levelPitchOffset, 2);

  Serial.println();
}

// ============================================================
// ANGLE DIFFERENCE
// ============================================================

float angleDifference(float a, float b)
{
  float diff = a - b;

  while (diff > 180.0f)
    diff -= 360.0f;

  while (diff < -180.0f)
    diff += 360.0f;

  return diff;
}

// ============================================================
// ROLL POLAR COORDINATES
// ============================================================

void rollPoint(
  float radius,
  float angle,
  float &x,
  float &y
)
{
  float r =
    angle * PI / 180.0f;

  x =
    CX + sin(r) * radius;

  y =
    CY - cos(r) * radius;
}

// ============================================================
// HORIZON
// ============================================================

void drawHorizon()
{
  horizonSprite.fillSprite(SKY);

  float pitchPixels =
    pitch * 2.0f;

  float angle =
    roll * PI / 180.0f;

  float dx = cos(angle);
  float dy = sin(angle);

  float nx = -dy;
  float ny = dx;

  float centerX =
    CX + nx * pitchPixels;

  float centerY =
    CY + ny * pitchPixels;

  // ----------------------------------------------------------
  // Fill ground
  // ----------------------------------------------------------

  for (int y = 0; y < SCREEN_H; y++)
  {
    float localY =
      y - CY;

    float inside =
      PFD_RADIUS * PFD_RADIUS -
      localY * localY;

    if (inside <= 0)
      continue;

    float halfWidth =
      sqrt(inside);

    int xLeft =
      max(
        0,
        (int)(CX - halfWidth)
      );

    int xRight =
      min(
        SCREEN_W - 1,
        (int)(CX + halfWidth)
      );

    for (int x = xLeft; x <= xRight; x++)
    {
      float px =
        x - centerX;

      float py =
        y - centerY;

      float side =
        px * nx +
        py * ny;

      if (side > 0)
      {
        horizonSprite.drawPixel(
          x,
          y,
          GROUND
        );
      }
    }
  }

  // ----------------------------------------------------------
  // Horizon line
  // ----------------------------------------------------------

  float lx =
    cos(angle);

  float ly =
    sin(angle);

  float vx =
    centerX - CX;

  float vy =
    centerY - CY;

  float projection =
    vx * lx +
    vy * ly;

  float closestX =
    centerX -
    projection * lx;

  float closestY =
    centerY -
    projection * ly;

  float distX =
    closestX - CX;

  float distY =
    closestY - CY;

  float closestDist =
    sqrt(
      distX * distX +
      distY * distY
    );

  float remaining =
    sqrt(
      max(
        0.0f,
        PFD_RADIUS * PFD_RADIUS -
        closestDist * closestDist
      )
    );

  float x1 =
    closestX -
    lx * remaining;

  float y1 =
    closestY -
    ly * remaining;

  float x2 =
    closestX +
    lx * remaining;

  float y2 =
    closestY +
    ly * remaining;

  horizonSprite.drawLine(
    x1,
    y1,
    x2,
    y2,
    WHITE
  );
}

// ============================================================
// PITCH LADDER
// ============================================================

void drawPitchLadder()
{
  const float pixelsPerDegree = 2.0f;

  const int shortWidth = 16;
  const int longWidth = 26;

  float a =
    roll * PI / 180.0f;

  float c = cos(a);
  float s = sin(a);

  for (int deg = -30; deg <= 30; deg += 10)
  {
    if (deg == 0)
      continue;

    float vertical =
      -(deg - pitch) *
      pixelsPerDegree;

    float width =
      (abs(deg) == 20)
      ? longWidth
      : shortWidth;

    float x1 = -width;
    float y1 = vertical;

    float x2 = width;
    float y2 = vertical;

    float rx1 =
      x1 * c -
      y1 * s;

    float ry1 =
      x1 * s +
      y1 * c;

    float rx2 =
      x2 * c -
      y2 * s;

    float ry2 =
      x2 * s +
      y2 * c;

    horizonSprite.drawLine(
      CX + rx1,
      CY + ry1,
      CX + rx2,
      CY + ry2,
      WHITE
    );

    char label[8];

    sprintf(
      label,
      "%d",
      abs(deg)
    );

    float labelX =
      -(width + 10);

    float labelY =
      vertical;

    float rx =
      labelX * c -
      labelY * s;

    float ry =
      labelX * s +
      labelY * c;

    uint16_t textBackground;

    if (deg > 0)
      textBackground = SKY;
    else
      textBackground = GROUND;

    horizonSprite.setTextDatum(MC_DATUM);

    horizonSprite.setTextColor(
      WHITE,
      textBackground
    );

    horizonSprite.drawString(
      label,
      CX + rx,
      CY + ry
    );
  }

  horizonSprite.setTextDatum(TL_DATUM);
}

// ============================================================
// ROLL TICKS
// ============================================================

void drawRollTicks()
{
  const float outerRadius = 114.0f;
  const float minorRadius = 106.0f;
  const float majorRadius = 101.0f;

  const float tickAngles[] =
  {
    -60,
    -45,
    -30,
    -15,
      0,
     15,
     30,
     45,
     60
  };

  for (int i = 0; i < 9; i++)
  {
    float relativeAngle =
      tickAngles[i];

    float finalAngle =
      relativeAngle + roll;

    float inner =
      (i % 2 == 0)
      ? majorRadius
      : minorRadius;

    float x1, y1;
    float x2, y2;

    rollPoint(
      inner,
      finalAngle,
      x1,
      y1
    );

    rollPoint(
      outerRadius,
      finalAngle,
      x2,
      y2
    );

    horizonSprite.drawLine(
      x1,
      y1,
      x2,
      y2,
      WHITE
    );
  }
}

// ============================================================
// WHITE ROLL TRIANGLE
// ============================================================

void drawRollTriangle()
{
  horizonSprite.fillTriangle(
    CX,
    9,
    CX - 6,
    24,
    CX + 6,
    24,
    WHITE
  );
}

// ============================================================
// YELLOW AIRCRAFT
// ============================================================

void drawAircraft()
{
  horizonSprite.fillRect(
    48,
    CY - 2,
    48,
    5,
    YELLOW
  );

  horizonSprite.fillRect(
    144,
    CY - 2,
    48,
    5,
    YELLOW
  );

  horizonSprite.fillCircle(
    CX,
    CY,
    5,
    YELLOW
  );
}

// ============================================================
// DATA BOX
// ============================================================

void drawDataBox(
  int x,
  int width,
  const char *label,
  const char *value
)
{
  int y = 190;
  int h = 25;

  // Black background

  horizonSprite.fillRect(
    x,
    y,
    width,
    h,
    BLACK
  );

  // White border

  horizonSprite.drawRect(
    x,
    y,
    width,
    h,
    WHITE
  );

  // Text

  horizonSprite.setTextDatum(MC_DATUM);

  horizonSprite.setTextColor(
    WHITE,
    BLACK
  );

  horizonSprite.setTextSize(1);

  horizonSprite.drawString(
    label,
    x + width / 2,
    y + 6
  );

  horizonSprite.drawString(
    value,
    x + width / 2,
    y + 18
  );

  horizonSprite.setTextDatum(TL_DATUM);
}

// ============================================================
// DATA
// ============================================================

void drawData()
{
  // ----------------------------------------------------------
  // SPEED
  // ----------------------------------------------------------

  drawDataBox(
    48,
    36,
    "SPD",
    "23.6"
  );

  // ----------------------------------------------------------
  // HEADING
  // ----------------------------------------------------------

  char headingText[12];

  if (magOK)
  {
    snprintf(
      headingText,
      sizeof(headingText),
      "%03d",
      (int)heading
    );
  }
  else
  {
    strcpy(
      headingText,
      "---"
    );
  }

  drawDataBox(
    102,
    36,
    "HDG",
    headingText
  );

  // ----------------------------------------------------------
  // ALTITUDE
  // ----------------------------------------------------------

  char altText[12];

  if (bmpOK)
  {
    snprintf(
      altText,
      sizeof(altText),
      "%.1f",
      altitude
    );
  }
  else
  {
    strcpy(
      altText,
      "--.-"
    );
  }

  drawDataBox(
    156,
    36,
    "ALT",
    altText
  );
}

// ============================================================
// OUTER BORDER
// ============================================================

void drawBorder()
{
  horizonSprite.drawCircle(
    CX,
    CY,
    BORDER_RADIUS,
    WHITE
  );
}

// ============================================================
// COMPLETE PFD
// ============================================================

void drawPFD()
{
  drawHorizon();

  drawPitchLadder();

  drawRollTicks();

  drawAircraft();

  drawRollTriangle();

  drawData();

  drawBorder();

  horizonSprite.pushSprite(
    0,
    0
  );
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(115200);

  delay(500);

  Serial.println();
  Serial.println("==============================");
  Serial.println("ESP32 PFD");
  Serial.println("TFT_eSPI SPRITE VERSION");
  Serial.println("==============================");

  // ==========================================================
  // I2C
  // ==========================================================

  Wire.begin(
    21,
    22
  );

  Wire.setClock(400000);

  // ==========================================================
  // BMP280
  // ==========================================================

  if (bmp.begin(0x76))
  {
    bmpOK = true;

    Serial.println("BMP280 found!");

    bmp.setSampling(
      Adafruit_BMP280::MODE_NORMAL,
      Adafruit_BMP280::SAMPLING_X2,
      Adafruit_BMP280::SAMPLING_X16,
      Adafruit_BMP280::FILTER_X16,
      Adafruit_BMP280::STANDBY_MS_63
    );
  }
  else
  {
    bmpOK = false;

    Serial.println("BMP280 not found!");
  }

  // ==========================================================
  // HMC5883L
  // ==========================================================

  if (mag.begin())
  {
    magOK = true;

    Serial.println("HMC5883L found!");
  }
  else
  {
    magOK = false;

    Serial.println("HMC5883L not found!");
  }

  // ==========================================================
  // TFT
  // ==========================================================

  tft.init();

  tft.setRotation(0);

  tft.fillScreen(BLACK);

  // ==========================================================
  // SPRITE
  // ==========================================================

  horizonSprite.setColorDepth(8);

  bool spriteOK =
    horizonSprite.createSprite(
      SCREEN_W,
      SCREEN_H
    );

  if (!spriteOK)
  {
    Serial.println(
      "ERROR: Sprite creation failed!"
    );

    while (true)
    {
      delay(1000);
    }
  }

  Serial.println(
    "8-bit 240x240 sprite created."
  );

// ==========================================================
// MPU
// ==========================================================

setupMPU();

delay(200);

// Loading / calibration screen
drawLoadingScreen();

calibrateGyro();

calibrateLevel();

  // ==========================================================
  // TIMER
  // ==========================================================

  lastMicros = micros();

  // ==========================================================
  // FIRST FRAME
  // ==========================================================

  drawPFD();

  Serial.println();
  Serial.println("PFD running.");
  Serial.println();
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
  // ==========================================================
  // READ MPU
  // ==========================================================

  int16_t ax, ay, az;
  int16_t gx, gy, gz;

  if (!readMPU(
        ax,
        ay,
        az,
        gx,
        gy,
        gz))
  {
    return;
  }

  // ==========================================================
  // READ BMP280 ALTITUDE
  // ==========================================================

  if (bmpOK)
  {
    altitude =
      bmp.readAltitude(1013.25f);
  }

  // ==========================================================
  // READ HMC5883L HEADING
  // ==========================================================

  if (magOK)
  {
    sensors_event_t event;

    mag.getEvent(&event);

    float magX =
      event.magnetic.x;

    float magY =
      event.magnetic.y;

    heading =
      atan2(
        magY,
        magX
      ) * 180.0f / PI;

    // Convert negative angle to 0-360

    if (heading < 0)
      heading += 360.0f;

    if (heading >= 360.0f)
      heading -= 360.0f;
  }

  // ==========================================================
  // DELTA TIME
  // ==========================================================

  unsigned long now =
    micros();

  float dt =
    (now - lastMicros) /
    1000000.0f;

  lastMicros = now;

  if (dt <= 0.0f || dt > 0.1f)
    dt = 0.01f;

  // ==========================================================
  // ACCELEROMETER
  // ==========================================================

  float accelX =
    ax / 16384.0f;

  float accelY =
    ay / 16384.0f;

  float accelZ =
    az / 16384.0f;

  // Inverted direction
  // This is your current working configuration.

  float accelRoll =
    -atan2(
      accelY,
      accelZ
    ) * 180.0f / PI;

  float accelPitch =
    -atan2(
      -accelX,
      sqrt(
        accelY * accelY +
        accelZ * accelZ
      )
    ) * 180.0f / PI;

  accelRoll -=
    levelRollOffset;

  accelPitch -=
    levelPitchOffset;

  // ==========================================================
  // GYROSCOPE
  // ==========================================================

  float gyroX =
    gx / 131.0f -
    gyroBiasX;

  float gyroY =
    gy / 131.0f -
    gyroBiasY;

  float gyroZ =
    gz / 131.0f -
    gyroBiasZ;

  // ==========================================================
  // GYRO PREDICTION
  // ==========================================================

  float gyroRoll =
    roll -
    gyroX * dt;

  float gyroPitch =
    pitch -
    gyroY * dt;

  // ==========================================================
  // COMPLEMENTARY FILTER
  // ==========================================================

  float rollError =
    angleDifference(
      accelRoll,
      gyroRoll
    );

  float pitchError =
    angleDifference(
      accelPitch,
      gyroPitch
    );

  roll =
    gyroRoll +
    rollError * 0.02f;

  pitch =
    gyroPitch +
    pitchError * 0.02f;

  // ==========================================================
  // NORMALIZE ROLL
  // ==========================================================

  while (roll > 180.0f)
    roll -= 360.0f;

  while (roll < -180.0f)
    roll += 360.0f;

  // ==========================================================
  // LIMIT PITCH
  // ==========================================================

  if (pitch > 89.0f)
    pitch = 89.0f;

  if (pitch < -89.0f)
    pitch = -89.0f;

  // ==========================================================
  // SERIAL DEBUG
  // ==========================================================

  if (millis() - lastSerial >= 100)
  {
    lastSerial =
      millis();

    Serial.print("Roll: ");
    Serial.print(
      roll,
      1
    );

    Serial.print(
      " | Pitch: "
    );

    Serial.print(
      pitch,
      1
    );

    Serial.print(
      " | Heading: "
    );

    if (magOK)
    {
      Serial.print(
        heading,
        1
      );

      Serial.print("°");
    }
    else
    {
      Serial.print(
        "HMC5883 ERROR"
      );
    }

    Serial.print(
      " | Altitude: "
    );

    if (bmpOK)
    {
      Serial.print(
        altitude,
        1
      );

      Serial.print(" m");
    }
    else
    {
      Serial.print(
        "BMP280 ERROR"
      );
    }

    Serial.print(
      " | AccelRoll: "
    );

    Serial.print(
      accelRoll,
      1
    );

    Serial.print(
      " | GX: "
    );

    Serial.print(
      gyroX,
      1
    );

    Serial.print(
      " | GY: "
    );

    Serial.println(
      gyroY,
      1
    );
  }

  // ==========================================================
  // DRAW
  // ==========================================================

  drawPFD();
}