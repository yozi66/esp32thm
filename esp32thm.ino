#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <analogWrite.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <time.h>

#include "arduino_secrets.h"
#include "thingProperties.h"
#include "AverageTemp.h"

#define LED_BUILTIN 16
// LED is OFF at HIGH level
#define LED_OFF 255
// the LED is too bright, 15/255 duty is enough
#define LED_ON 240

#define OLED_SDA 5
#define OLED_SCL 4

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C // from i2c scan

#define ONE_WIRE_BUS 14
#define TEMPERATURE_PRECISION 12

#define ARRAY_LENGTH 3

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);
DeviceAddress oneWire_addr[ARRAY_LENGTH];

// number of valid addresses
int addr_count = 0;

// average temperatures
AverageTemp avgTemp[ARRAY_LENGTH];

CloudTemperatureSensor * sensors[] = { &t0, &t1, &t2 };

// the current time
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

// local time
struct tm * timeinfo;

// last second displayed
int lastsec;

// last minute displayed
int lastmin;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); // switch the LED off
  Wire.begin(OLED_SDA, OLED_SCL);
  Serial.begin(115200);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("SSD1306 allocation failed");
    for(;;); // Don't proceed, loop forever
  }

  display.setRotation(2);
  display.clearDisplay();
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK); // Draw white text and erase background to black
  display.setCursor(0, 24);    // Start at the fourth line of the display
  display.cp437(true);         // Use full 256 char 'Code Page 437' font

  getLocalTime();
  printLocalTime();

  ds18b20.begin();
  display.clearDisplay();
  display.setCursor(0, 24);    // Start at the fourth line of the display

  display.print("Found ");
  int deviceCount = ds18b20.getDeviceCount();
  display.print(deviceCount, DEC);
  display.println(" sensor(s)");
  for (addr_count = 0; addr_count < ARRAY_LENGTH && addr_count < deviceCount; addr_count++) {
    ds18b20.getAddress(oneWire_addr[addr_count], addr_count);
    printAddress(oneWire_addr[addr_count]);
  }
  getLocalTime();
  printLocalTime();
  delay(500);

  ds18b20.requestTemperatures();
  delay(750);
  for (int i = 0; i < addr_count; i++) {
    avgTemp[i].setTemp(ds18b20.getTempC(oneWire_addr[i]));
  }
  printTemperatures();
  getLocalTime();
  printLocalTime();

  // Defined in thingProperties.h
  initProperties();

  // Connect to Arduino IoT Cloud
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();  
  Serial.print("update");
}

void loop() {
  getLocalTime();
  if (timeinfo->tm_sec != lastsec) {
    analogWrite(LED_BUILTIN, LED_ON);   // turn the LED on
    Serial.print(".");
    ArduinoCloud.update(); // does not work if called once a minute only
    analogWrite(LED_BUILTIN, LED_OFF);   // turn the LED off
  
    printTemperatures();
    if (timeinfo->tm_min != lastmin) {
      Serial.print("done\n");
      for (int i = 0; i < addr_count; i++) {
          float value = avgTemp[i].getAvg();
          *sensors[i] = value;
          Serial.print("t");
          Serial.print(i);
          Serial.print("=");
          Serial.print(value,3);
          Serial.print("\n");
      }
      Serial.print("update");
    }
    printLocalTime(); // updates lastmin and lastsec
  }
}

void printTemperatures() {
  display.clearDisplay();

  for (int i = 0; i < addr_count; i++) {
    avgTemp[i].setTemp(ds18b20.getTempC(oneWire_addr[i]));
    int y_base = (i + 1) * 16;
    display.setCursor(0, y_base);
    display.println(avgTemp[i].temp_curr, 2);
  
    display.setCursor(36, y_base);
    display.setTextSize(2);
    display.print(avgTemp[i].temp_disp, 1);
    display.setTextSize(1);
    display.print("\xF8" "C");
  }
  ds18b20.requestTemperatures();
}

void getLocalTime() {
  time_t gmt = time(NULL);
  time_t local = gmt + gmtOffset_sec + daylightOffset_sec;
  timeinfo = localtime(&local);
}

/* http://www.cplusplus.com/reference/ctime/strftime/ */
void printLocalTime() {
  if (timeinfo->tm_year > 100) {
    display.setCursor(4, 0);
    display.print(timeinfo, "%Y ");
    display.setCursor(34, 0);
    display.setTextSize(2);
    display.print(timeinfo, "%R");
    display.setTextSize(1);
    display.setCursor(0, 8);
    display.print(timeinfo, "%m-%d");
  }
  display.setCursor(94,0);
  display.print(timeinfo, ":%S");
  display.display();
  lastsec = timeinfo->tm_sec;
  lastmin = timeinfo->tm_min;
}

// function to print a device address
void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    // zero pad the address if necessary
    if (deviceAddress[i] < 16) Serial.print("0");
    display.print(deviceAddress[i], HEX);
  }
  display.println();
  display.display();
  delay(500);
}
