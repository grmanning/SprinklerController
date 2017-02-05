/*
  This sketch controlls 4 relays that drive solenoid swicthes on spary heads to proect the house from busfire. It is designed to run in a Standby mode, Protect mode and Fire mode.

  A menu is availble to set timers and change modes.
  In Standby mode the controller monitors the temperature and automatically move to Fire mode if it rises too high.
  In Protect mode the controller cycles through each relay to spary water from one sprinkler at a time (saving water) and wetting down the area ahead of an expected fire.
  In Fire mode the controller turns on all 4 sprinklers until the fire passes or the timer runs out (again to save water).

  Vesions:
  1 - First working system 15/1/2016
  2 - Update to remove the blanking of the screen when idle due to it not un-blanking after a few hours.
  3 - Fix for blanking menu - use unsigned long for time. Reintroduced menu blanking and reduced screen delay to zero. Added idle timeout again, turns off backlight instead.
  4 - Convert to using Freetronics Realtime Clock
  5 - Retain state in NVRAM to recovery from power outage
  6 - Serial comms to Processing sketch to log data and be remotely controlled

*/

//#define debug // uncomment to turn on debugging output to serial port
//#define debugNVRAM
//#define testmode  // switch to test parameters


#include "IRTemp.h"
#include <stdlib.h>
#include <string.h>
#include <Wire.h>
#include "RTClib.h"
#include <LiquidCrystal.h>
#include <SoftI2C.h>
#include <DS3232RTC.h>



#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5

#ifdef testmode
#define twohours 120         // Short test period
#define cyclePeriod 10      // Short cycle time between sprinkler heads
#define fireThreshold 27    // Low temperature (C) to set off fire mode from IR Sensor
#else
#define twohours 7200       // Normal default test period
#define cyclePeriod 180     // Normal 2 minutes for a sprinkler head before cycling
#define fireThreshold 60   // High temperature (C) to set off fire mode from IR Sensor
#endif

#define fourhours 14400     // 4 hour timelimit for unattended Protection
#define sixhours 21600      // 6 hour timelimit for unattended Protection
#define eighthours 28800    // 8 hour timelimit for unattended Protection

// Define menus  (value is unimportant, just unique)
#define topMenu 1
#define timerMenu 2
#define blankMenu 3

// Define menu items (value is unimportant, just unique)
#define setTimers 2
#define standby 3
#define startProtect 4
#define startFire 5

// Define modes
#define standbyMode 1
#define protectMode 2
#define fireMode 3

// Define timings
#define cycleDelayTime 500       // time delay between solenoids when powering on more than one
#define checkTempSeconds 5       // time delay before checking temperature again (improves UI feedback)
#define displayRefreshSeconds 0  // time delay before refreshing the display (improves UI feedback)
#define displayIdleSeconds 120   // maximum time of no button presses before blanking the display
#define saveTimeDelay 300    // period to save time remaining of fire/protect mode to memory for recovery

// Define pins for solenoids
int sol1 = 2;
int sol2 = 3;
int sol3 = A2;
int sol4 = A1;

// Set default mode and timeout period for spraying
int mode = standbyMode;
int timeoutPeriod = twohours;



// Set scale for temperture conversion
static const TempUnit SCALE = CELSIUS; // Options are CELSIUS, FAHRENHEIT

// State of relays now (with index) and last cycle
int relayno = 0;
int relay [4] = {0, 0, 0, 0};
int lastRelay [4] = {0, 0, 0, 0};

// Define display variables
char currentTimeText [9];
char modeText = 'S';
char timeoutText = '2';
char IRText [5];
char AmbText [5];
char menuText [17];
char countdown [8];


// Define variables to navigate menus
int currentMenu;
int currentItem;

// Define timing variables
RTC_Millis RTC;     // Real time clock
unsigned long stopTime;       // Expiry time for spraying timeout
unsigned long  cycleTime;      // Expiry time for cycling to next solenoid/spray head
unsigned long  checkTempTime = 0L;  //  Expiry time to check temperature next
unsigned long  idleTime = 0L;       //  Expiry time to blank display when idle
unsigned long  displayRefreshTime = 0L; // Expiry time to next display refresh
unsigned long  saveTime = 0L; // Expiry time to next save of remaininf time to NVRAM


DateTime now;       // General variable to grab current time


// Define pins for IR Temperature sensor and initialise
static const byte PIN_DATA    = 12;
static const byte PIN_CLOCK   = 11;
static const byte PIN_ACQUIRE = A3;
IRTemp irTemp(PIN_ACQUIRE, PIN_CLOCK, PIN_DATA);

// select the pins used on the LCD panel and initialise
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// Define date time info and select the pins
SoftI2C i2c(A4, A5);
DS3232RTC rtc(i2c);

const char *days[] = {
  "Mon, ", "Tue, ", "Wed, ", "Thu, ", "Fri, ", "Sat, ", "Sun, "
};

const char *months[] = {
  " Jan ", " Feb ", " Mar ", " Apr ", " May ", " Jun ",
  " Jul ", " Aug ", " Sep ", " Oct ", " Nov ", " Dec "
};

// Define NVRAM parameters for recovering from a power outage

int lastMode;
unsigned long lastStopTime;


// Define flag for controlling when to send data to the PC
int ready2Send;


/*
   Start of code section
*/

// dumpNVRAM displays the contents of NVRAM in hex.
void dumpNVRAM()
{
  static const char hexchars[] = "0123456789ABCDEF";
  int count = rtc.byteCount();
  for (int offset = 0; offset < count; ++offset) {
    if ((offset % 16) == 0) {
      if (offset)
        Serial.println();
      Serial.print(hexchars[(offset >> 12) & 0x0F]);
      Serial.print(hexchars[(offset >> 8) & 0x0F]);
      Serial.print(hexchars[(offset >> 4) & 0x0F]);
      Serial.print(hexchars[offset & 0x0F]);
      Serial.print(':');
      Serial.print(' ');
    }
    byte value = rtc.readByte(offset);
    Serial.print(hexchars[(value >> 4) & 0x0F]);
    Serial.print(hexchars[value & 0x0F]);
    Serial.print(' ');
  }
  Serial.println();
}

void checkMemory () {
  /*
    Read the NVRAM to see if if there was a prior mode to recover
  */
  DateTime now;
  int minutesLeft;

#ifdef debugNVRAM
  dumpNVRAM();
#endif
  lastMode = 0;
  lastMode = (int) rtc.readByte(0);

  minutesLeft = (int) rtc.readByte(1);
  if (minutesLeft == 0) {
    minutesLeft = timeoutPeriod / 60;
  }

  now = getDateTime();
  if (lastMode == protectMode) {
    modeText = 'P';
    mode = protectMode;
    stopTime = now.unixtime() + (minutesLeft * 60);
    cycleTime = now.unixtime() + cyclePeriod;
    relayno = 0;
    relay[0] = 1;
    relay[1] = 0;
    relay[2] = 0;
    relay[3] = 0;
  }
  if (lastMode == fireMode) {
    mode = fireMode;
    modeText = 'F';
    stopTime = now.unixtime() + (minutesLeft * 60);
    relay[0] = 1;
    relay[1] = 1;
    relay[2] = 1;
    relay[3] = 1;
  }
}


/*
   Declare RTC functions
*/

DateTime getDateTime() {
  RTCTime time;
  RTCDate date;

  rtc.readTime(&time);
  rtc.readDate(&date);
  DateTime currentDateTime(date.year, date.month, date.day, time.hour, time.minute, time.second);
  return currentDateTime;
}


void getDateTimeString() {
  RTCTime time;
  char hourText [3];
  char minutesText[3];
  char secondsText[3];

  rtc.readTime(&time);

  itoa(time.hour, hourText, 10);
  itoa(time.minute, minutesText, 10);
  itoa(time.second, secondsText, 10);

  strcpy(currentTimeText, hourText);
  strcat(currentTimeText, ":");
  if (time.minute < 10) {
    strcat(currentTimeText, "0");
  }
  strcat(currentTimeText, minutesText);
  strcat(currentTimeText, ":");
  if (time.second < 10) {
    strcat(currentTimeText, "0");
  }
  strcat(currentTimeText, secondsText);

}


char* convertTime2String(DateTime datetime) {

  char hourText [3];
  char minutesText[3];
  char secondsText[3];


  itoa(datetime.hour(), hourText, 10);
  itoa(datetime.minute(), minutesText, 10);
  itoa(datetime.second(), secondsText, 10);

  strcpy(currentTimeText, hourText);
  strcat(currentTimeText, ":");
  if (datetime.minute() < 10) {
    strcat(currentTimeText, "0");
  }
  strcat(currentTimeText, minutesText);
  strcat(currentTimeText, ":");
  if (datetime.second() < 10) {
    strcat(currentTimeText, "0");
  }
  strcat(currentTimeText, secondsText);
}


void displayDateTime() {
  RTCTime time;
  RTCDate date;
  rtc.readTime(&time);
  rtc.readDate(&date);

  Serial.print("Time: ");
  printDec2(time.hour);
  Serial.print(':');
  printDec2(time.minute);
  Serial.print(':');
  printDec2(time.second);
  Serial.println();

  Serial.print("Date: ");
  Serial.print(days[RTC::dayOfWeek(&date) - 1]);
  Serial.print(date.day, DEC);
  Serial.print(months[date.month - 1]);
  Serial.print(date.year);
  Serial.println();

  Serial.print("Temp: ");
  int temp = rtc.readTemperature();
  if (temp != RTC::NO_TEMPERATURE) {
    Serial.print(temp / 4.0);
    Serial.println(" celcius");
  } else {
    Serial.println("not available");
  }

  Serial.println();

}

void printDec2(int value)
{
  Serial.print((char)('0' + (value / 10)));
  Serial.print((char)('0' + (value % 10)));
}



int read_LCD_buttons()
/*
   If a button is being pressed, record which it is and wait for the button to be released.
   Return the button identifier.
*/
{
  int adc_key_in  = 0;
  int adc_key_held  = 0;
  adc_key_in = analogRead(0);
  // my buttons when read are centered at these valies: 0, 144, 329, 504, 741
  // we add approx 50 to those values and check to see if we are close
  if (adc_key_in > 1000) return btnNONE; // We make this the 1st option for speed reasons since it will be
  // For V1.1 us this threshold
  // if (adc_key_in < 50)   return btnRIGHT;
  // if (adc_key_in < 250)  return btnUP;
  // if (adc_key_in < 450)  return btnDOWN;
  // if (adc_key_in < 650)  return btnLEFT;
  // if (adc_key_in < 850)  return btnSELECT;
  // For V1.0 comment the other threshold and use the one below:

  adc_key_held = analogRead(0);
  while (adc_key_held < 790) {
    lcd.noDisplay();
    delay (50);
    adc_key_held = analogRead(0);
  }

  if (adc_key_in < 50)   return btnRIGHT;
  if (adc_key_in < 195)  return btnUP;
  if (adc_key_in < 380)  return btnDOWN;
  if (adc_key_in < 555)  return btnLEFT;
  if (adc_key_in < 790)  return btnSELECT;

  return btnNONE;
}


void checkButtonPress() {
  /*
     checks which buttons are pressed and navigates the menu.
  */
  int lcd_key     = 0;
  DateTime now;

  now =  getDateTime();  // Freetronics RTC

  lcd_key = read_LCD_buttons();
  if (lcd_key == btnNONE) {
    if (idleTime < now.unixtime()) {
      pinMode(10, OUTPUT);  // Backlight off
#ifdef debug
      Serial.print(idleTime);
      Serial.print("<");
      Serial.print(now.unixtime());
#endif
    }
    return;
  }
  pinMode(10, INPUT); // Backlight on
  idleTime = now.unixtime() + displayIdleSeconds;
  displayRefreshTime = now.unixtime();
  switch (currentMenu) {
    case topMenu: {
        switch (currentItem) {
          case setTimers: {
              switch (lcd_key) {
                case btnUP: {
                    currentItem = standby;
                    strcpy(menuText, "Standby");
                    return;
                  }
                case btnDOWN:
                  {
                    currentItem = startProtect;
                    strcpy(menuText, "Protect");
                    return;
                  }
                case btnSELECT:
                  {
                    currentMenu = timerMenu;
                    currentItem = twohours;
                    strcpy(menuText, "2 Hours");
                    timeoutText = '2';
                    return;
                  }
              }
            }
          case startProtect: {
              switch (lcd_key) {
                case btnUP: {
                    currentItem = setTimers;
                    strcpy(menuText, "Timers");
                    return;
                  }
                case btnDOWN:
                  {
                    currentItem = startFire;
                    strcpy(menuText, "All On");
                    return;
                  }
                case btnSELECT:
                  {
                    currentMenu = topMenu;
                    currentItem = setTimers;
                    mode = protectMode;
                    strcpy(menuText, "Timers");
                    modeText = 'P';
                    stopTime = now.unixtime() + timeoutPeriod;
                    cycleTime = now.unixtime() + cyclePeriod;
                    relayno = 0;
                    relay[0] = 1;
                    relay[1] = 0;
                    relay[2] = 0;
                    relay[3] = 0;
                    rtc.writeByte(0, protectMode);
                    return;
                  }
              }
            }
          case startFire: {
              switch (lcd_key) {
                case btnUP: {
                    currentItem = startProtect;
                    strcpy(menuText, "Protect");
                    return;
                  }
                case btnDOWN:
                  {
                    currentItem = standby;
                    strcpy(menuText, "Standby");
                    return;
                  }
                case btnSELECT:
                  {
                    currentMenu = topMenu;
                    currentItem = setTimers;
                    mode = fireMode;
                    strcpy(menuText, "Timers");
                    modeText = 'F';
                    stopTime = now.unixtime() + timeoutPeriod;
                    relay[0] = 1;
                    relay[1] = 1;
                    relay[2] = 1;
                    relay[3] = 1;
                    rtc.writeByte(0, fireMode);
                    return;

                  }
              }
            }
          case standby: {
              switch (lcd_key) {
                case btnUP: {
                    currentItem = startFire;
                    strcpy(menuText, "All On");
                    return;
                  }
                case btnDOWN:
                  {
                    currentItem = setTimers;
                    strcpy(menuText, "Timers");
                    return;
                  }
                case btnSELECT:
                  {
                    currentMenu = topMenu;
                    currentItem = setTimers;
                    mode = standbyMode;
                    strcpy(menuText, "Timers");
                    modeText = 'S';
                    relay[0] = 0;
                    relay[1] = 0;
                    relay[2] = 0;
                    relay[3] = 0;
                    rtc.writeByte(0, standbyMode);
                    return;

                  }
              }

            }
        }
      }
    case timerMenu: {
        switch (currentItem) {
          case twohours: {
              switch (lcd_key) {
                case btnUP: {
                    currentItem = eighthours;
                    strcpy(menuText, "8 Hours");
                    return;
                  }
                case btnDOWN:
                  {
                    currentItem = fourhours;
                    strcpy(menuText, "4 Hours");
                    return;
                  }
                case btnSELECT:
                  {
                    currentMenu = topMenu;
                    currentItem = setTimers;
                    timeoutPeriod = twohours;
                    timeoutText = '2';
                    strcpy(menuText, "Timers");
                    return;
                  }
              }
            }
          case fourhours: {
              switch (lcd_key) {
                case btnUP: {
                    currentItem = twohours;
                    strcpy(menuText, "2 Hours");
                    return;
                  }
                case btnDOWN:
                  {
                    currentItem = sixhours;
                    strcpy(menuText, "6 Hours");
                    return;
                  }
                case btnSELECT:
                  {
                    currentMenu = topMenu;
                    currentItem = setTimers;
                    timeoutPeriod = fourhours;
                    strcpy(menuText, "Timers");
                    timeoutText = '4';
                    return;

                  }
              }
            }
          case sixhours: {
              switch (lcd_key) {
                case btnUP: {
                    currentItem = fourhours;
                    strcpy(menuText, "4 hours");
                    return;
                  }
                case btnDOWN:
                  {
                    currentItem = eighthours;
                    strcpy(menuText, "8 hours");
                    return;
                  }
                case btnSELECT:
                  {
                    currentMenu = topMenu;
                    currentItem = setTimers;
                    timeoutPeriod = sixhours;
                    strcpy(menuText, "Timers");
                    timeoutText = '6';
                    return;

                  }
              }
            }
          case eighthours: {
              switch (lcd_key) {
                case btnUP: {
                    currentItem = sixhours;
                    strcpy(menuText, "6 hours");
                    return;
                  }
                case btnDOWN:
                  {
                    currentItem = twohours;
                    strcpy(menuText, "2 hours");
                    return;
                  }
                case btnSELECT:
                  {
                    currentMenu = topMenu;
                    currentItem = setTimers;
                    timeoutPeriod = eighthours;
                    strcpy(menuText, "Timers");
                    timeoutText = '8';
                    return;
                  }
              }
            }
        }
      }

      //    case blankMenu: {
      //        currentMenu = topMenu;
      //        currentItem = setTimers;
      //        strcpy(menuText, "Timers");
      //      }

  }
}

void checkFireTimeout() {
  /*
       If the timeout period has expired, set all relay states to off and return to standby mode.
  */

  unsigned long  stopTimeSecondsSince1970;
  unsigned long  nowSecondsSince1970;
  int seconds;
  int minutes;
  int hours;
  int tot;
  byte stopMinutes;

  char hourText [2];
  char minutesText[3];
  char secondsText[3];

  DateTime now;

  now =  getDateTime();  // Freetronics RTC

  nowSecondsSince1970 = now.unixtime();
  if ((mode == protectMode) || (mode == fireMode)) {
    if (stopTime < nowSecondsSince1970) {

      relay[0] = 0;
      relay[1] = 0;
      relay[2] = 0;
      relay[3] = 0;
      mode = standbyMode;
      modeText = 'S';
      rtc.writeByte(0, standbyMode);

#ifdef debug
      Serial.print("Timeout reached");
      Serial.print(relay[0]);
      Serial.print(relay[1]);
      Serial.print(relay[2]);
      Serial.println(relay[3]);
#endif
    }
    else {
      if (saveTime < nowSecondsSince1970) {
        saveTime = nowSecondsSince1970 + saveTimeDelay;
        stopMinutes = (stopTime - nowSecondsSince1970) / 60;  // Enable recovery of up to 255 minutes remaining time
        rtc.writeByte(1, stopMinutes);
      }
      tot = stopTime - nowSecondsSince1970;
      seconds = tot % 60;
      minutes = (tot / 60) % 60;
      hours = tot / 3600;

      itoa(hours, hourText, 10);
      itoa(minutes, minutesText, 10);
      itoa(seconds, secondsText, 10);

      strcpy(countdown, hourText);
      strcat(countdown, ":");
      if (minutes < 10) {
        strcat(countdown, "0");
      }
      strcat(countdown, minutesText);
      strcat(countdown, ":");
      if (seconds < 10) {
        strcat(countdown, "0");
      }
      strcat(countdown, secondsText);
    }
  }
}
void checkCycleTimeout() {
  /*
     If in Protect mode, change to the next relay every minute.
  */
  unsigned long  cycleTimeSecondsSince1970;
  unsigned long  nowSecondsSince1970;
  DateTime now;
  if (mode == protectMode) {
    now =  getDateTime();  // Freetronics RTC
    nowSecondsSince1970 = now.unixtime();
    if (cycleTime < nowSecondsSince1970) {
#ifdef debug
      Serial.print("Cycling up");
#endif
      cycleTime = now.unixtime() + cyclePeriod;
      relay[relayno] = 0;
      relayno++;
      if (relayno > 3) {
        relayno = 0;
      }
      relay[relayno] = 1;
#ifdef debug
      Serial.print(relay[0]);
      Serial.print(relay[1]);
      Serial.print(relay[2]);
      Serial.println(relay[3]);
#endif
    }
  }

}


void printTemperature(
  char  *type,
  float  temperature) {

  Serial.print(type);
  Serial.print(" temperature: ");

  if (isnan(temperature)) {
    Serial.print("Failed");
  }
  else {
    Serial.print(temperature);
    Serial.print(SCALE == FAHRENHEIT  ?  " F"  :  " C");
  }
}


void CheckTemp() {
  DateTime now;



  now =  getDateTime();  // Freetronics RTC

#ifdef debug
  Serial.print(now.unixtime());
  Serial.print("<");
  Serial.print(checkTempTime);
  Serial.print("?");
#endif

  if (checkTempTime < now.unixtime()) {
    checkTempTime = now.unixtime() + checkTempSeconds;

    float irTemperature = irTemp.getIRTemperature(SCALE);
    float ambientTemperature = irTemp.getAmbientTemperature(SCALE);

    itoa(irTemperature, IRText, 10);
    strcat(IRText, SCALE == FAHRENHEIT  ?  " F"  :  " C");
    itoa(ambientTemperature, AmbText, 10);
    strcat(AmbText, SCALE == FAHRENHEIT  ?  " F"  :  " C");

#ifdef debug
    Serial.print(" ");
    printTemperature((char *) "IR", irTemperature);
    printTemperature((char *) "Ambient", ambientTemperature);
#endif
    if (!isnan(irTemperature)) {
    int irTemp = abs(irTemperature);
    int irTempFrac = (float) (abs(irTemperature) - irTemp) * 100;
    if (ready2Send == 1) {
      ready2Send = 0;
      Serial.write('?');
      Serial.write('T');
      if (irTemperature < 0) {
        Serial.write('-');
      }
      else {
        Serial.write('+');
      }
      Serial.write((char) irTemp);
      Serial.write((char) irTempFrac);
      Serial.write('$');
    }
    }
    if (mode != fireMode) {
      if (irTemperature > fireThreshold) {
        mode = fireMode;
        modeText = 'F';
        now =  getDateTime();  // Freetronics RTC
        stopTime = now.unixtime()  + timeoutPeriod;
        relay[0] = 1;
        relay[1] = 1;
        relay[2] = 1;
        relay[3] = 1;
        rtc.writeByte(0, fireMode);
      }
    }
  }
}

void powerRelays() {
  /*
     sets the relays to what the previous functions have defined.
  */
  int delaytime = 0;
  if (relay[0] + relay[1] + relay[2] + relay[3] > 1) {
    delaytime = cycleDelayTime;
  }

  digitalWrite(sol1, relay[0]);
  if (lastRelay[0] != relay[0]) {
    delay(delaytime);
  }
  digitalWrite(sol2, relay[1]);
  if (lastRelay[1] != relay[1]) {
    delay(delaytime);
  }
  digitalWrite(sol3, relay[2]);
  if (lastRelay[2] != relay[2]) {
    delay(delaytime);
  }
  digitalWrite(sol4, relay[3]);
  lastRelay[0] = relay[0];
  lastRelay[1] = relay[1];
  lastRelay[2] = relay[2];
  lastRelay[3] = relay[3];
}


void reportMode() {
  if (ready2Send == 1) {
    Serial.write('?');
    Serial.write('M');
    Serial.write(mode - 1);
    Serial.write('$');
    ready2Send = 0;
  }
}


void refreshDisplay() {
  /*
     sets the display to what the previous functions have defined.
  */
  char txt [17];

  DateTime now;
  //  Serial.print("RefreshDisplay:");
  //  Serial.println(menuText);
  //  Serial.print("Mode: ");
  //  Serial.print(mode);
  //  Serial.print("Timeout: ");
  //  Serial.println(timeoutPeriod);


  //  if ((currentMenu == blankMenu) && !((mode == fireMode) || (mode == protectMode))  ) {
  //    lcd.noDisplay();
  //    return;
  //  }

  now =  getDateTime();  // Freetronics RTC
  if (displayRefreshTime < now.unixtime()) {
    reportMode();
#ifdef debug
    displayDateTime();
    Serial.print(displayRefreshTime);
    Serial.print("<");
    Serial.print(now.unixtime());
    Serial.println("REFRESHED");
#endif
    displayRefreshTime = now.unixtime() + displayRefreshSeconds;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.display();
    lcd.print(menuText);
    if ((mode == fireMode) || (mode == protectMode)) {
      lcd.setCursor(9, 0);
      lcd.print(countdown);
    }
    else {
      getDateTimeString();
      lcd.setCursor(8, 0);
      lcd.print(currentTimeText);
    }
    lcd.setCursor(0, 1);
    lcd.print(modeText);
    lcd.setCursor(2, 1);
    lcd.print(timeoutText);
    lcd.setCursor(3, 1);
    lcd.print("H");
    lcd.setCursor(5, 1);
    lcd.print(IRText);
    lcd.setCursor(11, 1);
    lcd.print(AmbText);
#ifdef testmode
    lcd.setCursor(0, 1);
    lcd.blink();
#endif
  }
}

void checkComms() {
#ifndef debug
  if (Serial.available()) {
    int inByte = Serial.read();
    if (inByte == '$') {
        ready2Send = 1;   // Flag it is OK to send data (PC is ready to receive now)
    }
    if (inByte == 0) {
      // Enter Standby mode
      relay[0] = 0;
      relay[1] = 0;
      relay[2] = 0;
      relay[3] = 0;
      mode = standbyMode;
      modeText = 'S';
      rtc.writeByte(0, standbyMode);
    }
    if (inByte == 1) {
      // Enter Protect mode
      mode = protectMode;
      modeText = 'P';
      stopTime = now.unixtime() + timeoutPeriod;
      cycleTime = now.unixtime() + cyclePeriod;
      relayno = 0;
      relay[0] = 1;
      relay[1] = 0;
      relay[2] = 0;
      relay[3] = 0;
      rtc.writeByte(0, protectMode);
    }
    if (inByte == 2) {
      // Enter AllOn mode
      mode = fireMode;
      modeText = 'F';
      now =  getDateTime();  // Freetronics RTC
      stopTime = now.unixtime()  + timeoutPeriod;
      relay[0] = 1;
      relay[1] = 1;
      relay[2] = 1;
      relay[3] = 1;
      rtc.writeByte(0, fireMode);
    }
  }
#endif
}


void setup() {
  // put your setup code here, to run once:
  /*
     starts the program and finishes setup of devices.
  */
  Serial.begin(9600);
  pinMode(sol1, OUTPUT);
  pinMode(sol2, OUTPUT);
  pinMode(sol3, OUTPUT);
  pinMode(sol4, OUTPUT);

  /*
      Set the bcklight control to LOW ready to turn off by setting pinMode to OUTPUT when idle.
      Begin by setting piMode to INPUT to have D10 not grounded, which turns on the backlight. This
      is done rather than using HIGH output because several displays have a bad design that shorts D10
      putting high load on the pin which may damage the board over time.
  */
  digitalWrite(10, LOW);  // Ready to ground later when idle by setting pinMode to OUTPUT
  pinMode(10, INPUT);     // Set pinMode to input now to light up the display to start with
  
#ifndef debug
  ready2Send = 1;
#endif
  currentMenu = topMenu;
  currentItem = setTimers;
  strcpy(menuText, "Timers");
  lcd.begin(16, 2);
  now =  getDateTime();  // Freetronics RTC
  idleTime = now.unixtime() + displayIdleSeconds;
  pinMode(10, INPUT); // Backlight on
  saveTime = now.unixtime();
  checkTempTime = now.unixtime();
  checkMemory();
}

void loop() {
  // put your main code here, to run repeatedly:
  /*
     makes everything work by calling the functions.
  */
#ifdef debug
  Serial.print("C1,");
#endif
  checkButtonPress();   // action command if entered
#ifdef debug
  Serial.print("C2,");
#endif
  checkFireTimeout();   // clear relay states if timeout expired to save water
#ifdef debug
  Serial.print("C3,");
#endif
  checkCycleTimeout();  // cycle to next relay if timeout on current relay
#ifdef debug
  Serial.print("C4,");
#endif
  CheckTemp();          // check if mode must change due to temperature
  checkComms();         // check if the remote controller has a new state to change to
#ifdef debug
  Serial.print("C5,");
#endif
  powerRelays();        // send power to the correct relays
#ifdef debug
  Serial.print("C6");
#endif
  refreshDisplay();     // show the new state and menu or clear if idle
  // reportMode();         // report the current mode back to PC
  // checkComms();
  delay(250);
}
