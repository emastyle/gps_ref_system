/*********************************************************************
Simple GPS System - SCULLCOM derived version
Adapted for OLED SSD1306 (128x64 I2C)
With LED fix indicator, satellite icon, NMEA debug output
*********************************************************************/

// --- Include libraries ---
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SoftwareSerial.h>
#include <TinyGPS.h>

// --- Prototipi delle funzioni ---
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


// --- LED blink state ---
bool ledState = false;
unsigned long lastBlink = 0;

bool hasSatFixed() {
  float lat, lon;
  unsigned long fix_age, time_age;
  
  gps.f_get_position(&lat, &lon, &fix_age);
  gps.get_datetime(NULL, NULL, &time_age);

  // Check multiple conditions for a valid GPS fix:
  // 1. Valid coordinates (not invalid angles)
  bool hasValidCoords = !(lat == TinyGPS::GPS_INVALID_F_ANGLE ||
                         lon == TinyGPS::GPS_INVALID_F_ANGLE);
  
  // 2. Fresh GPS data (fix age less than 5 seconds)
  bool hasFreshData = (fix_age != TinyGPS::GPS_INVALID_AGE && fix_age < 5000);
  
  // 3. Sufficient satellites (at least 4 for reliable 3D fix)
  unsigned char sats = gps.satellites();
  bool hasSatellites = (sats != TinyGPS::GPS_INVALID_SATELLITES && sats >= 4);
  
  // 4. All conditions must be true for a valid fix
  return hasValidCoords && hasFreshData && hasSatellites;
}

// --- LED handling ---
void handleLedStatus(bool hasFix) {
  if (hasFix) {
    digitalWrite(LED_PIN, HIGH); // steady ON when fix
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

  // Always handle LED (non-blocking)
  bool hasFix = hasSatFixed();
  handleLedStatus(hasFix);

  // Continuously feed GPS data
  if (feedgps())
    newdata = true;

  // Every 1 second, refresh display with same fix status as LED
  if (millis() - lastGpsUpdate >= 1000) {
    gpsdump(gps, hasFix); // Pass the same hasFix value used for LED
    if (newdata) {
      newdata = false;
    } else {
      Serial.println(F("No valid GPS data yet..."));
    }
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

  // --- Serial debug output ---
  float flat, flon;
  unsigned long fix_age, time_age;
  gps.f_get_position(&flat, &flon, &fix_age);
  gps.get_datetime(NULL, NULL, &time_age);
  unsigned char sats = gps.satellites();
  
  bool hasValidCoords = !(flat == TinyGPS::GPS_INVALID_F_ANGLE || flon == TinyGPS::GPS_INVALID_F_ANGLE);
  bool hasFreshData = (fix_age != TinyGPS::GPS_INVALID_AGE && fix_age < 5000);
  bool hasSatellites = (sats != TinyGPS::GPS_INVALID_SATELLITES && sats >= 4);
  
  Serial.println(F("------ GPS DATA ------"));
  Serial.print(F("Satellites: ")); Serial.println(sats);
  Serial.print(F("Latitude  : ")); Serial.println(flat, 6);
  Serial.print(F("Longitude : ")); Serial.println(flon, 6);
  Serial.print(F("Fix age   : ")); Serial.print(fix_age); Serial.println(F(" ms"));
  Serial.print(F("Valid coords: ")); Serial.println(hasValidCoords ? F("YES") : F("NO"));
  Serial.print(F("Fresh data  : ")); Serial.println(hasFreshData ? F("YES") : F("NO"));
  Serial.print(F("Enough sats : ")); Serial.println(hasSatellites ? F("YES") : F("NO"));
  Serial.print(F("Fix status  : ")); Serial.println(hasSatFixed() ? F("FIXED") : F("NO FIX"));
  Serial.println(F("----------------------"));
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
  display.println(gps.satellites());
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
    display.println(sats);
  } else {
    display.println(F("0"));
  }
}

// --- Read GPS and echo raw NMEA data ---
bool feedgps() {
  bool newData = false;

  while (nss.available()) {
    char c = nss.read();

    // TinyGPS decode
    if (gps.encode(c))
      newData = true;

    // Print raw NMEA characters
    Serial.write(c);
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
