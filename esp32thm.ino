#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <analogWrite.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>

#include "arduino_secrets.h"
#include "time.h"

#define OLED_SDA 5
#define OLED_SCL 4
#define LED_BUILTIN 16
// LED is OFF at HIGH level
#define LED_OFF 255
// the LED is too bright, 55/255 duty is enough
#define LED_ON 200

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C // from i2c scan

#define ONE_WIRE_BUS 14
#define TEMPERATURE_PRECISION 12

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);

const char* wifi_ssid = SECRET_SSID;
const char* wifi_pass = SECRET_PASS;

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

// the current time
struct tm timeinfo;
bool valid_time;

// last second displayed
int lastsec;

// thermometer
DeviceAddress oneWire_addr;

// last measured temperature
float temp_curr;

// exponential moving average temperature
float temp_avg;

// weight of the old value in exponential moving average
const int old_wt = 7;

// temperature on display
float temp_disp;

// display hysteresis in Celsius
const float hysteresis = 0.05;

// temperature change direction, true for increase and false for decrease
boolean temp_inc;

void blink() {
    analogWrite(LED_BUILTIN, LED_ON);   // turn the LED on
    delay(500);                       // wait for 1/2 second
    analogWrite(LED_BUILTIN, LED_OFF);  // turn the LED off
    delay(500);                      // wait for 1/2 second  
}

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
  display.print("Connecting to ");
  display.println(wifi_ssid);
  display.display();

  WiFi.begin(wifi_ssid, wifi_pass);
 
  while (WiFi.status() != WL_CONNECTED) {
    blink();
    Serial.println("Establishing connection to WiFi..");
  }
 
  Serial.println("Connected to network");
  analogWrite(LED_BUILTIN, LED_ON);
  display.println("Connected");
  display.println();
  display.println("Getting time from ");
  display.println(ntpServer);
  display.display();
  delay(250);

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();
  if(!getLocalTime(&timeinfo)){
    display.println("Failed to obtain time");
    display.display();
  }
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(250);

  ds18b20.begin();
  display.clearDisplay();
  display.setCursor(0, 24);    // Start at the fourth line of the display

  display.print("Found ");
  display.print(ds18b20.getDeviceCount(), DEC);
  display.println(" sensor(s)");
  ds18b20.getAddress(oneWire_addr, 0);
  printAddress(oneWire_addr);
  valid_time = getLocalTime(&timeinfo);
  printLocalTime();
  ds18b20.requestTemperatures();
  delay(750);
  temp_avg = ds18b20.getTempC(oneWire_addr);
  temp_disp = temp_avg;
  printTemperature();
  printLocalTime();
}

void loop() {
  // put your main code here, to run repeatedly:
  valid_time = getLocalTime(&timeinfo);
  if (!valid_time) {
    delay(750);
  }
  if (!valid_time || timeinfo.tm_sec != lastsec) {
    printTemperature();
    printLocalTime();
  }
}

void printTemperature() {
  display.clearDisplay();

  // current temperature
  temp_curr = ds18b20.getTempC(oneWire_addr);
  display.setCursor(0, 56);
  display.print("Raw: ");
  display.println(temp_curr, 4);
  
  // average temperature
  temp_avg = (temp_curr + old_wt * temp_avg) / (old_wt + 1); // compute moving average
  /*
  display.setCursor(0, 46);
  display.print("Avg: ");
  display.println(temp_avg, 6);
  */

  // temperature with hysteresis
  bool update = false;
  if (temp_inc) {
    update = temp_avg > temp_disp;
    if (!update && temp_avg < temp_disp - hysteresis) {
      update = true;
      temp_inc = false; 
    }
  } else {
     update = temp_avg < temp_disp;
     if (!update && temp_avg > temp_disp + hysteresis) {
      update = true;
      temp_inc = true;
     }
  }
  if (update) {
    temp_disp = temp_avg;
  }
  display.setCursor(20, 25);
  display.setTextSize(3);
  display.print(temp_disp, 1);
  display.setTextSize(1);
  display.print("\xF8" "C");
  ds18b20.requestTemperatures();
}
/* http://www.cplusplus.com/reference/ctime/strftime/ */
void printLocalTime() {
  if(valid_time){
    display.setCursor(4, 0);
    display.print(&timeinfo, "%Y ");
    display.setCursor(34, 1);
    display.setTextSize(2);
    display.print(&timeinfo, "%R");
    display.setTextSize(1);
    display.print(&timeinfo, ":%S");
    display.setCursor(0, 10);
    display.print(&timeinfo, "%m-%d");
    display.display();
    lastsec = timeinfo.tm_sec;
  } else {
    display.setCursor(0, 0);     // Start at top-left corner
    display.println("????-??-?? ??:??:??");
  }
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
}
