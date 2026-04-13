/*********************************************************************
GPS REF System - Derived from SCULLCOM version
Adapted for OLED SSD1306 (128x64 I2C)
With LED fix indicator, NMEA debug output
*********************************************************************/

// --- Include libraries ---
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SoftwareSerial.h>
#include <TinyGPS.h>

// --- Function prototypes ---
void handleLedStatus(bool hasFix);
bool feedgps();
void gpsdump(TinyGPS &gps, bool hasFix);
void displayGpsData(TinyGPS &gps);
void displayWaitingMessage(TinyGPS &gps);
static void print_date(TinyGPS &gps);

// --- Display configuration ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define I2C_ADDRESS 0x3C

// --- LED pin ---
#define LED_PIN 7   // LED indicator pin

// --- Global objects ---
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
TinyGPS gps;
SoftwareSerial nss(3, 4);  // D3 = RX, D4 = TX (optional)

bool hasSatFixed() {
  float lat, lon;
  unsigned long fix_age, time_age;
  
  gps.f_get_position(&lat, &lon, &fix_age);
  gps.get_datetime(NULL, NULL, &time_age);

  // Check multiple conditions for a valid GPS fix:
  // 1. Valid coordinates (not invalid angles)
  bool hasValidCoords = !(lat == TinyGPS::GPS_INVALID_F_ANGLE ||
                         lon == TinyGPS::GPS_INVALID_F_ANGLE);
  
  // 2. Fresh GPS data (fix age less than 10 seconds - more lenient)
  bool hasFreshData = (fix_age != TinyGPS::GPS_INVALID_AGE && fix_age < 10000);
  
  // 3. Sufficient satellites (at least 3 for basic fix - more lenient)
  unsigned char sats = gps.satellites();
  bool hasSatellites = (sats != TinyGPS::GPS_INVALID_SATELLITES && sats >= 3);
  
  // 4. All conditions must be true for a valid fix
  return hasValidCoords && hasFreshData && hasSatellites;
}

// --- LED handling ---
void handleLedStatus(bool hasFix) {
  if (hasFix) {
    digitalWrite(LED_PIN, HIGH);
  } else {
    digitalWrite(LED_PIN, LOW); 
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println(F("GPS SYSTEM - DEBUG START"));

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  nss.begin(9600);
  Serial.println(F("GPS serial started at 9600 baud"));

  // OLED init
  if (!display.begin(SSD1306_SWITCHCAPVCC, I2C_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  // Speed up I2C to reduce OLED transfer time (100kHz→400kHz saves ~70ms per frame)
  Wire.setClock(400000L);

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println(F("GPS SYSTEM"));

  display.setTextSize(1);
  display.setCursor(0, 18);
  display.println(F("EMANUELE GIAN - 2025"));

  display.setCursor(0, 30);
  display.println(F("Initialising..."));

  display.display();
  delay(2000);
  display.clearDisplay();
}

void loop() {
  static unsigned long lastGpsUpdate = 0;
  static bool newdata = false;

  // Feed GPS data first, then evaluate fix on fresh data
  if (feedgps())
    newdata = true;

  bool hasFix = hasSatFixed();
  handleLedStatus(hasFix);

  // Every 1 second, refresh display
  if (millis() - lastGpsUpdate >= 1000) {
    gpsdump(gps, hasFix);
    newdata = false;
    lastGpsUpdate = millis();
  }
}

// --- Print GPS data on OLED ---
void gpsdump(TinyGPS &gps, bool hasFix) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (hasFix) {
    displayGpsData(gps);
  } else {
    displayWaitingMessage(gps);
  }

  // Clear any remaining area at bottom of screen to remove artifacts
  display.fillRect(0, 56, 128, 8, SSD1306_BLACK);
  display.display();

  // --- Serial debug output (compact to avoid SoftwareSerial buffer overflow) ---
  float flat, flon;
  unsigned long fix_age;
  gps.f_get_position(&flat, &flon, &fix_age);
  unsigned char sats = gps.satellites();

  Serial.print(F("Sats:")); Serial.print(sats);
  Serial.print(F(" Age:")); Serial.print(fix_age);
  Serial.print(F("ms Fix:")); Serial.println(hasFix ? F("YES") : F("NO"));
}

// --- Display GPS data when fixed ---
void displayGpsData(TinyGPS &gps) {
  float flat, flon;
  unsigned long fix_age;
  gps.f_get_position(&flat, &flon, &fix_age);

  // Date/time
  display.setCursor(0, 0);
  print_date(gps);

  // Altitude
  display.setCursor(0, 18);
  display.print(F("Altitude: "));
  display.print(gps.f_altitude());
  display.println(F(" m"));

  // Latitude
  display.setCursor(0, 28);
  display.print(F("Lat: "));
  display.print(flat, 4);

  // Longitude  
  display.setCursor(0, 38);
  display.print(F("Lon: "));
  display.print(flon, 4);

  // Satellites
  display.setCursor(0, 48);
  display.print(F("Sats: "));
  display.print(gps.satellites());

  // Uptime
  unsigned long up = millis() / 1000;
  display.setCursor(70, 48);
  display.print(F("Up:"));
  display.print(up / 60);
  display.print(F("m"));
  display.print(up % 60);
  display.print(F("s"));
}

// --- Display waiting message when no fix ---
void displayWaitingMessage(TinyGPS &gps) {
  unsigned char sats = gps.satellites();
  
  display.setCursor(5, 18);
  display.println(F("Waiting for"));
  display.setCursor(5, 28);
  display.println(F("satellites..."));
  
  // Show satellite count to indicate progress
  display.setCursor(5, 40);
  display.print(F("Sats: "));
  if (sats != TinyGPS::GPS_INVALID_SATELLITES) {
    display.print(sats);
  } else {
    display.print(F("0"));
  }

  // Uptime
  unsigned long up = millis() / 1000;
  display.setCursor(70, 40);
  display.print(F("Up:"));
  display.print(up / 60);
  display.print(F("m"));
  display.print(up % 60);
  display.print(F("s"));
}

// --- Read GPS and echo raw NMEA data ---
bool feedgps() {
  bool newData = false;

  while (nss.available()) {
    char c = nss.read();

    // TinyGPS decode
    if (gps.encode(c))
      newData = true;
  }

  return newData;
}

// --- Format date/time ---
static void print_date(TinyGPS &gps) {
  int year;
  byte month, day, hour, minute, second;
  gps.crack_datetime(&year, &month, &day, &hour, &minute, &second);

  char sz[32];
  sprintf(sz, "%02d/%02d/%02d %02d:%02d:%02d UTC",
          day, month, year % 100, hour, minute, second);

  display.print(sz);
}
