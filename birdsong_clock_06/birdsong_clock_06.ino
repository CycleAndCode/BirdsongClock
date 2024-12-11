/*
This code requires mp3 files present on the mp3 player microSD card
SD card
|- folder named 01 - 99
|---- file named 001.mp3 - 255.mp3
To play this files, add them somewhere in the schedules below.
DO NOT USE OTHER NAMES unles you know what you are doing (read the DFplayer documentation for more options)
*/
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <RTClib.h>
#include "Arduino.h"
#include "SoftwareSerial.h"
#include <DFPlayerMini_Fast.h>
#include <OneWire.h>
#include <DallasTemperature.h>
extern "C" { 
  #include "user_interface.h" 
}

int time_zone_offset = 1; //UTC + 1

// Data wire is connected to D0 on ESP8266 (GPIO4)
#define ONE_WIRE_BUS 13 // D7
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);
// Pass oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// Define RX and TX pins for software serial
// #define RX_PIN 4  //D2 on ESP8266
// #define TX_PIN 5  //D1 on ESP8266
#define BUSY_PIN 16 //D0
#define MP3_ENABLE_PIN 15  // D8
//hardware Serial1 is on D4 (TX only). Connect it to software TX pin D1
bool is_dfplayer_hardware_serial1 = true;  //OBSOLETE use software serial for testing/debugging and hardware for normal run
bool disable_serial0 = true;   //disables printing for power save
int last_temperature_update_min = -1;   //minute of last temperature update, used to reduce the amount of temp readings
int last_date_update_day = -1;  //day of last date update 

// Create a SoftwareSerial object
// SoftwareSerial mySoftwareSerial(RX_PIN, TX_PIN);

DFPlayerMini_Fast myDFPlayer;
// void printDetail(uint8_t type, int value);
// void initialize_DFPlayer();

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET     -1   // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C // OLED I2C address
#define OLED_SDA 14         // SDA pin (D5)
#define OLED_SCL 12         // SCL pin (D6)

const char* ssid     = "Tech_D0058680"; // Replace with your WiFi SSID
const char* password = "HBKKCQQR";      // Replace with your WiFi password

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000); // UTC time, update every 60 seconds

Adafruit_SSD1306 *display;
RTC_DS1307 rtc;
bool DST_is_set = false;
// bool DFPlayer_awake = false; //keep tracking of the player's status to manage it properly

// Structure to hold schedule entries
struct ScheduleEntry {
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t folder;
  int songID;
};
// Array of special entries
ScheduleEntry spec_schedule[] = {
  {1, 1, 0, 0, 13, 1},   
  {4, 2, 21, 37, 13, 2},  
  {6, 22, 5, 0, 13, 3},
  {11, 11, 11, 11, 13, 4}, 
  {12, 24, 18, 0, 13, 5},
  {12, 25, 9, 0, 13, 6}, 
};
//Array of normal entries
ScheduleEntry daily_schedule[] = {
  {0, 0, 6, 0, 1, 6}, 
  {0, 0, 7, 0, 1, 7}, 
  {0, 0, 8, 0, 1, 8},
  {0, 0, 9, 0, 1, 9}, 
  {0, 0, 10, 0, 1, 10}, 
  {0, 0, 11, 0, 1, 11}, 
  {0, 0, 12, 0, 1, 12}, 
  {0, 0, 13, 0, 1, 13}, 
  {0, 0, 14, 0, 1, 14}, 
  {0, 0, 15, 0, 1, 15}, 
  {0, 0, 16, 0, 1, 16}, 
  {0, 0, 17, 0, 1, 17},
  {0, 0, 18, 0, 1, 18}, 
  {0, 0, 19, 0, 1, 19}, 
  {0, 0, 20, 0, 1, 20}, 
  {0, 0, 21, 0, 1, 21}, 
};

//variables
ScheduleEntry NextPlayPoint, NPP1, NPP2;
bool play_point_done = true;

void setup() {
  Serial.begin(9600);
  Serial.println("Starting DST Tests...");
  testDST(); // Test DST logic

  // Initialize OLED display
  display = new Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display->begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("SSD1306 allocation failed");
    for (;;); // Stop here if display initialization fails
  }
  display->clearDisplay();

  // Initialize the DS18B20 temperature sensor
  sensors.begin();

  unsigned long timeout = 10000; // Timeout duration in milliseconds (10 seconds)
  sync_RTC_from_WiFi(ssid, password, timeout);
  adjustRTCForDST(); // Ensure RTC is in sync with DST

  pinMode(BUSY_PIN, INPUT_PULLUP);
  pinMode(MP3_ENABLE_PIN, OUTPUT);
  // initialize_DFPlayer(is_dfplayer_hardware_serial1);
  //play welcome slong
  play_a_song(4, 5, 1, 1);

  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  Serial.println("Wi-Fi is turned off to save power.");

  if(disable_serial0){
      Serial.end();   //disable printing
  }
}


void loop() {
  // if(digitalRead(MP3_ENABLE_PIN) == HIGH){
  //   if(digitalRead(BUSY_PIN) == HIGH){
  //     myDFPlayer.volume(0);
  //     delay(100);
  //     digitalWrite(MP3_ENABLE_PIN, LOW);
  //   }
  // }  
  DateTime now = rtc.now();
  display_time(now);  
  if(play_point_done){
    NPP1 = findNextSchedule_daily(now);
    print_next_event(NPP1);
    NPP2 = findNextSchedule(now);
    print_next_event(NPP2);
    NextPlayPoint = select_closest_entry(now, NPP1, NPP2);
    print_next_event(NextPlayPoint);
    display_next_time(NextPlayPoint);
    play_point_done = false;
  }
  if (now.minute() == NextPlayPoint.minute &&
    now.hour() == NextPlayPoint.hour &&
    now.day() == NextPlayPoint.day &&
    now.month() == NextPlayPoint.month){
    play_a_song(5, 25, NextPlayPoint.folder, NextPlayPoint.songID);
    play_point_done = true;
  }

  ls_delay(950); //avoid the display seconds skipping too often
}

void print_next_event(ScheduleEntry nextEvent){
    Serial.print("Next event: Day ");
    Serial.print(nextEvent.day);
    Serial.print(", Month ");
    Serial.print(nextEvent.month);
    Serial.print(", Time ");
    Serial.print(nextEvent.hour);
    Serial.print(":");
    Serial.print(nextEvent.minute);
    Serial.print(", folder: ");
    Serial.print(nextEvent.folder);
    Serial.print(", songID:");
    Serial.println(nextEvent.songID);
}

ScheduleEntry select_closest_entry(DateTime now, ScheduleEntry NPP1, ScheduleEntry NPP2){
  ScheduleEntry nextEvent = {0, 0, 0, 0, -1};
  uint32_t minDifference = UINT32_MAX;

  ScheduleEntry schedules[] = {NPP1, NPP2};
  for (int i = 0; i < 2; i++) {
    DateTime eventDate(now.year(), schedules[i].month, schedules[i].day, schedules[i].hour, schedules[i].minute);

    if (eventDate < now) {
      // If the event date is in the past, add a year
      eventDate = DateTime(now.year() + 1, schedules[i].month, schedules[i].day, 
                  schedules[i].hour, schedules[i].minute);
    }

    uint32_t difference = (eventDate.unixtime() - now.unixtime());

    if (difference < minDifference) {
      minDifference = difference;
      nextEvent = schedules[i];
    }
  }
  return nextEvent;
}

// Function to find the next schedule entry
ScheduleEntry findNextSchedule(DateTime now) {
  ScheduleEntry nextEvent = {0, 0, 0, 0, -1};
  uint32_t minDifference = UINT32_MAX;

  for (int i = 0; i < sizeof(spec_schedule) / sizeof(spec_schedule[0]); i++) {
    DateTime eventDate(now.year(), spec_schedule[i].month, spec_schedule[i].day,
                       spec_schedule[i].hour, spec_schedule[i].minute);

    if (eventDate < now) {
      // If the event date is in the past, add a year
      eventDate = DateTime(now.year() + 1, spec_schedule[i].month, spec_schedule[i].day,
                           spec_schedule[i].hour, spec_schedule[i].minute);
    }

    uint32_t difference = (eventDate.unixtime() - now.unixtime());

    if (difference < minDifference) {
      minDifference = difference;
      nextEvent = spec_schedule[i];
    }
  }
  return nextEvent;
}

ScheduleEntry findNextSchedule_daily(DateTime now) {
  ScheduleEntry nextEvent = {0, 0, 0, 0, -1};
  uint32_t minDifference = UINT32_MAX;

  for (int i = 0; i < sizeof(daily_schedule) / sizeof(daily_schedule[0]); i++) {
    DateTime eventDate(now.year(), now.month(), now.day(),
                       daily_schedule[i].hour, daily_schedule[i].minute);

    if (eventDate < now) {
      // If the event date is in the past, add one day
      eventDate = eventDate + TimeSpan(1, 0, 0, 0); // Add 1 day (1 day, 0 hours, 0 minutes, 0 seconds) 
    }

    uint32_t difference = (eventDate.unixtime() - now.unixtime());

    if (difference < minDifference) {
      minDifference = difference;
      nextEvent = daily_schedule[i];
      nextEvent.month = eventDate.month();
      nextEvent.day = eventDate.day();
      nextEvent.folder = 1;   //so boring, but play from one folder for now     
      // nextEvent.folder = pick_right_season(now); 
    }
  }
  return nextEvent;
}

//Useful if you want to organize your music according to 4 seasons
int pick_right_season(DateTime now){ 
  // {3, 21}, //first day of spring
  // {6, 22}, //first day of summer
  // {9, 23}, //first day of autumn
  // {12, 22}, //first day of winter
  int day = now.day();
  int month = now.month();
  int month100xday = month*100 + day;
  // int N = sizeof(season_select) / sizeof(season_select[0];
    if(month100xday < 321){ return(4);}
    if(month100xday < 622){ return(1);}
    if(month100xday < 923){ return(2);}
    if(month100xday < 1222){ return(3);
    }else{ return(4);}
}

void sync_RTC_from_WiFi(const char* ssid, const char* password, unsigned long timeout) {
  unsigned long startAttemptTime = millis();

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - startAttemptTime >= timeout) {
      Serial.println("Failed to connect to WiFi. Using existing RTC time.");
      return;
    }
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC. Using existing RTC time.");
    return;
  }

  // Sync RTC from NTP on startup
  syncRTCFromNTP();
}

// Mainly for testing
void nice_delay_s(int seconds){
  for(int i = 0; i<seconds; i++){
    display_time(rtc.now());
    ls_delay(950); //avoid the display seconds skipping too often
  }
}

// Custom Light Sleep delay function
void ls_delay(unsigned long milliseconds) {
  // Configure Wi-Fi to NULL_MODE to ensure it's off
  wifi_set_opmode(NULL_MODE);
  // Set up Light Sleep
  wifi_fpm_set_sleep_type(LIGHT_SLEEP_T); // Set to Light Sleep mode
  wifi_fpm_open(); // Open Force Sleep API
  // Enter Light Sleep for the specified duration
  wifi_fpm_do_sleep(milliseconds * 1000); // Time is in microseconds, so 1000 ms = 1,000,000 µs
  // Force the chip to sleep
  delay(milliseconds); // Temporary fix to ensure sleep occurs
  wifi_fpm_close(); // Close Force Sleep API after waking up
}

//Play a song with rising volume
void play_a_song(int min_vol, int max_vol, int folder_num, int track_num){
    Serial1.begin(9600);
    digitalWrite(MP3_ENABLE_PIN, HIGH);
    delay(200);
    while(!myDFPlayer.begin(Serial1)) {  
      Serial.println(F("Unable to begin:"));
      Serial.println(F("1.Please recheck the connection!"));
      Serial.println(F("2.Please insert the SD card!"));
    }
    myDFPlayer.playFolder(folder_num, track_num);
    Serial.print(F("Now playing folder number: "));
    Serial.print(folder_num);
    Serial.print(F(", file number: "));
    Serial.print(track_num);
    Serial.println(F("."));
    for(int i = min_vol; i<max_vol; i++){
        myDFPlayer.volume(i);  //Set volume value. From 0 to 30
        delay(200);
        display_time(rtc.now());
    }    
    while(digitalRead(BUSY_PIN)==0){
        ls_delay(1000);
        display_time(rtc.now());
    }
    myDFPlayer.volume(0);  //Set volume value. From 0 to 30      
    Serial1.end();
    digitalWrite(MP3_ENABLE_PIN, LOW);
}

void testDST() {
  // DST stands for daylight saving time
  Serial.println("Testing DST Logic...");

  // Test cases for key dates
  DateTime testDates[] = {
    DateTime(2024, 3, 24, 1, 0, 0),  // One week before DST starts
    DateTime(2024, 3, 31, 1, 59, 59), // Just before DST starts
    DateTime(2024, 3, 31, 2, 0, 0),   // DST starts
    DateTime(2024, 6, 15, 12, 0, 0),  // Middle of DST
    DateTime(2024, 10, 27, 2, 59, 59), // Just before DST ends
    DateTime(2024, 10, 27, 3, 0, 0),   // DST ends
    DateTime(2024, 12, 15, 12, 0, 0),  // Middle of Standard Time
  };
    /*
2024-03-24 01:00:00: DST = No (CET)
2024-03-31 01:59:59: DST = No (CET)
2024-03-31 02:00:00: DST = Yes (CEST)
2024-06-15 12:00:00: DST = Yes (CEST)
2024-10-27 02:59:59: DST = Yes (CEST)
2024-10-27 03:00:00: DST = No (CET)
2024-12-15 12:00:00: DST = No (CET)
*/

  for (int i = 0; i < sizeof(testDates) / sizeof(testDates[0]); i++) {
    DateTime current = testDates[i];
    bool dstResult = isDST(current);

    Serial.print("Date: ");
    Serial.print(current.year());
    Serial.print("-");
    Serial.print(current.month());
    Serial.print("-");
    Serial.print(current.day());
    Serial.print(" ");
    Serial.print(current.hour());
    Serial.print(":");
    Serial.print(current.minute());
    Serial.print(":");
    Serial.print(current.second());
    Serial.print(" -> DST: ");
    Serial.println(dstResult ? "Yes (CEST)" : "No (CET)");
  }
}

// Function to check if it's DST
bool isDST(DateTime now) {
  // DST is active from the last Sunday in March to the last Sunday in October
  if (now.month() > 3 && now.month() < 10) {
    return true;
  }

  if (now.month() == 3) {
    // Find last Sunday in March
    DateTime lastSunday = DateTime(now.year(), 3, 31, 2, 0, 0);
    while (lastSunday.dayOfTheWeek() != 0) {
      lastSunday = lastSunday - TimeSpan(1, 0, 0, 0);
    }
    // DST starts at 2:00 AM on the last Sunday in March
    return now >= lastSunday;
  }

  if (now.month() == 10) {
    // Find last Sunday in October
    DateTime lastSunday = DateTime(now.year(), 10, 31, 3, 0, 0);
    while (lastSunday.dayOfTheWeek() != 0) {
      lastSunday = lastSunday - TimeSpan(1, 0, 0, 0);
    }
    // DST ends at 3:00 AM on the last Sunday in October
    return now < lastSunday;
  }
  return false;
}

void syncRTCFromNTP() {
  while (!timeClient.update()) {
    timeClient.forceUpdate();
  }

  // Get UTC time from NTP
  time_t ntpTime = timeClient.getEpochTime();
  DateTime nowUTC = DateTime(ntpTime);

  // Add time offset (UTC +1 by default)
  DateTime nowLocal = nowUTC + TimeSpan(time_zone_offset * 3600); // UTC +1

  // Check if DST applies
  bool dstNow = isDST(nowLocal);
  if (dstNow) {
    nowLocal = nowLocal + TimeSpan(1 * 3600); // Add 1 hour for DST (UTC +2)
  }

  // Update RTC and DST flag
  rtc.adjust(nowLocal);
  DST_is_set = dstNow;

  Serial.print("RTC updated from NTP with time zone offset of ");
  Serial.print(time_zone_offset);
  Serial.println(" hour");
}

void adjustRTCForDST() {
  DateTime now = rtc.now();
  bool dstNow = isDST(now);

  if (dstNow != DST_is_set) {
    // DST mismatch, adjust the RTC time
    if (dstNow) {
      now = now + TimeSpan(1 * 3600); // Add 1 hour for DST
    } else {
      now = now - TimeSpan(1 * 3600); // Subtract 1 hour for standard time
    }

    rtc.adjust(now);  // DateTime now = rtc.now(); // Get the local time
    DST_is_set = dstNow;

    Serial.println("RTC adjusted for DST change");
  }
}

void display_time(DateTime now) {
  int X = 0;
  int Y = 22;
  int month = now.month();
  if( month == 3 || month == 10){
    adjustRTCForDST(); // Ensure RTC is in sync with DST
  }
  // display->clearDisplay();
  // Clear specific area 
  display->fillRect(X, Y, 128, 24, SSD1306_BLACK);

  display->setTextSize(3);
  display->setTextColor(SSD1306_WHITE);
  display->setCursor(X, Y);

  // Display the current time
  if (now.hour() < 10) display->print("0");
  display->print(now.hour());
  display->setTextSize(3);    
  display->print(":");
  display->setTextSize(3);  
  if (now.minute() < 10) display->print("0");
  display->print(now.minute());
  display->setTextSize(2);  
  display->print(":");
  if (now.second() < 10) display->print("0");
  display->print(now.second());
  display->display();
  if(now.minute() != last_temperature_update_min){
    display_temperature();   
    last_temperature_update_min = now.minute();
  }
  if(now.day() != last_date_update_day){   // no need to update the date too frequently
    last_date_update_day = now.day();
    X = 0;
    Y = 56;
    display->fillRect(X, Y, 59, 8, SSD1306_BLACK);
    display->setTextSize(1); 
    display->setCursor(X, Y);
    if (now.day() < 10) display->print("0");
    display->print(now.day());   
    display->print(".");
    if (now.month() < 10) display->print("0");
    display->print(now.month());
    display->display();
  }

}

void display_temperature(){
  sensors.requestTemperatures();
  // Fetch the temperature value in Celsius
  float temperatureC = sensors.getTempCByIndex(0);
  // Clear specific area 
  display->fillRect(0, 0, 128, 16, SSD1306_BLACK);
  display->setTextSize(2);
  display->setTextColor(SSD1306_WHITE);
  display->setCursor(24, 0);
  display->print(temperatureC);
  display->print(F(" C"));
  display->display();
}

void display_next_time(ScheduleEntry next) {
  // set start coordinates
  int X = 60;
  int Y = 56;
  display->fillRect(X, Y, 64, 8, SSD1306_BLACK);
  display->setTextSize(1);
  display->setTextColor(SSD1306_WHITE);
  display->setCursor(X, Y);
  if (next.day < 10) display->print("0");
  display->print(next.day);   
  display->print(".");
  if (next.month < 10) display->print("0");
  display->print(next.month);
  display->print(" ");
  if (next.hour < 10) display->print("0");
  display->print(next.hour);
  display->print(":");
  if (next.minute < 10) display->print("0");
  display->print(next.minute);
  display->display();
}
