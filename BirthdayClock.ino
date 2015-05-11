// Copyright (C) 2015 Derek Chafin

// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version.

// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public
// License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

/*

  My Age Stats
  Seconds
  Minutes
  Hours
  Days
  Years

  Calculate my age
  Use current time of day to calculate seconds from midnight for hours minutes and seconds of age.
  Get offset from my actual birthday and calculate my age stats

  typedef struct  {
    uint8_t Second;
    uint8_t Minute;
    uint8_t Hour;
    uint8_t Wday;   // day of week, sunday is day 1
    uint8_t Day;
    uint8_t Month;
    uint8_t Year;   // offset from 1970;
  }   tmElements_t, TimeElements, *tmElementsPtr_t;

  The time and date functions can take an optional parameter for the time. This prevents
  errors if the time rolls over between elements. For example, if a new minute begins
  between getting the minute and second, the values will be inconsistent. Using the
  following functions eliminates this probglem
  int     hour(time_t t);    // the hour for the given time
  int     hourFormat12(time_t t); // the hour for the given time in 12 hour format
  uint8_t isAM(time_t t);    // returns true the given time is AM
  uint8_t isPM(time_t t);    // returns true the given time is PM
  int     minute(time_t t);  // the minute for the given time
  int     second(time_t t);  // the second for the given time
  int     day(time_t t);     // the day for the given time
  int     weekday(time_t t); // the weekday for the given time
  int     month(time_t t);   // the month for the given time
  int     year(time_t t);    // the year for the given time

  1.  Jan - January
  2.  Feb - February
  3.  Mar - March
  4.  Apr - April
  5.  May - May
  6.  Jun - June
  7.  Jul - July
  8.  Aug - August
  9.  Sep - September
  10. Oct - October
  11. Nov - November
  12. Dec - December
*/

#include <SimpleTimer.h>
// include the library code:
#include <LiquidCrystal.h>

#include <Time.h>
#include <Wire.h>
#include <DS1307RTC.h>  // a basic DS1307 library that returns time as a time_t
#include "Adafruit_MCP9808.h"

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);
// Wednesday 22nd May 1985 5:00:00 AM UTC
TimeElements birthdayElem = { 0, 0, 5, 4, 22, 5, 15};
SimpleTimer timer;
Adafruit_MCP9808 tempsensor;

int timerDisplayTime;
int timerDisplayAgeStats;
int timerDisplayTemperature;

time_t currentTime;
time_t birthdayTime;

#define UPDATETIME_INTERVAL 1000
#define DISPLAY_TIME_INTERVAL 500
#define DISPLAY_AGESTATS_INTERVAL 2000
#define DISPLAY_TEMPERATURE_INTERVAL 1000
#define RUNTIME_RESOLUTION 25
#define MAX_RUNTIME 8000
#define MAX_RUNS (MAX_RUNTIME / RUNTIME_RESOLUTION)
#define IS_DST true
const int TZ = -6;
/*
  SECS_PER_HOUR = (3600UL);
  def now():
    # get UTC time here

  # If the RTC is set to UTC
  def get_localtime():
    # time_t here is a 32bit integer
    time_t utc = now();
    localtime = utc + (TZ * SECS_PER_HOUR);
    if (IS_DST):
      localtime += SECS_PER_HOUR;

    return localtime;

  print get_localtime();
 */

void setup() {
  // Pin 13 is a floater so explicitly turn it off.
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);

  // Set backlight brightness
  pinMode(10, OUTPUT);
  analogWrite(10, 127);

  birthdayTime = makeTime(birthdayElem);
  birthdayTime = getLocalTime(birthdayTime, TZ, IS_DST);

  byte customChar[8] = {
    0b01100,
    0b10010,
    0b10010,
    0b01100,
    0b00000,
    0b00000,
    0b00000,
    0b00000
  };

  // create a new custom character
  lcd.createChar(0, customChar);

  // set up number of columns and rows
  lcd.begin(16, 2);

  displayBootMessage();

  if (!tempsensor.begin()) {
    lcd.print(F("No Temperature!"));
    while(1);
  }

  setSyncProvider(RTC.get);   // the function to get the time from the RTC
  setSyncInterval(SECS_PER_MIN);
  // No individual function can be longer than this interval.
  timer.setInterval(UPDATETIME_INTERVAL, updateTime);
  timer.setInterval(RUNTIME_RESOLUTION, changeTimer);

  timerDisplayTime = timer.setInterval(DISPLAY_TIME_INTERVAL, displayTime);
  timer.disable(timerDisplayTime);

  timerDisplayAgeStats = timer.setInterval(DISPLAY_AGESTATS_INTERVAL, displayAgeStats);
  timer.disable(timerDisplayAgeStats);

  timerDisplayTemperature = timer.setInterval(DISPLAY_TEMPERATURE_INTERVAL, displayTemperature);
  timer.disable(timerDisplayTemperature);

  updateTime();
  changeTimer();
}

void loop() {
  timer.run();
}

void updateTime() {
  if (timeStatus() == timeSet) {
    currentTime = now(); // store the current time in time variable t
  }
  else {
    // Saturday 1st January 2000 12:00:00 AM UTC
    currentTime = 946684800UL;
  }

  currentTime = getLocalTime(currentTime, TZ, IS_DST);
}

void changeTimer() {
  static int timerReference = -1;
  static int function = 0;
  static int numRuns = 0;

  if (numRuns++ >= MAX_RUNS) {
    numRuns = 0;

    if (timerReference >= 0) {
      timer.disable(timerReference);
      timerReference = -1;
    }

    switch (function) {
      // Display Current Time
      case 0:
        displayTime();
        timerReference = timerDisplayTime;
        timer.enable(timerDisplayTime);
        function = 1;
        break;

      // Display Age Stats
      case 1:
        displayAgeStats();
        timerReference = timerDisplayAgeStats;
        timer.enable(timerDisplayAgeStats);
        function = 2;
        break;

      // Display Current Temperature
      case 2:
        displayTemperature();
        timerReference = timerDisplayTemperature;
        timer.enable(timerDisplayTemperature);
        function = 0;
        break;

      default:
        function = 0;
        break;
    }
  }

}

void displayBootMessage() {
  lcd.clear();
  lcd.setCursor(0, 0);

  lcd.print(F("Birthday Clock"));
  lcd.setCursor(0, 1);
  lcd.print(F("by Derek Chafin"));
}

void displayTime() {
  static boolean blinkColon = true;

  lcd.clear();
  lcd.setCursor(0, 0);

  // May 22, 2015
  lcd.print(monthShortStr(month(currentTime)));
  lcd.print(' ');
  lcd.print(day(currentTime));
  lcd.print(", ");
  lcd.print(year(currentTime));

  // Start at beginning of second line
  lcd.setCursor(0, 1);

  // 12:25 PM
  displayLeadingCharacter(hourFormat12(currentTime), ' ');
  if (blinkColon)
    lcd.print(':');
  else
    lcd.print(' ');
  blinkColon = !blinkColon;
  displayLeadingCharacter(minute(currentTime), '0');

  lcd.print(' ');
  if (isPM(currentTime)) {
    lcd.print(F("PM"));
  }
  else {
    lcd.print(F("AM"));
  }

}

void displayAgeStats() {
  static int function = 0;

  time_t ageTime;
  TimeElements ageElem;

/*
  #define SECS_PER_MIN  (60UL)
  #define SECS_PER_HOUR (3600UL)
  #define SECS_PER_DAY  (SECS_PER_HOUR * 24UL)
  #define DAYS_PER_WEEK (7UL)
  #define SECS_PER_WEEK (SECS_PER_DAY * DAYS_PER_WEEK)
  #define SECS_PER_YEAR (SECS_PER_WEEK * 52UL)

  Useful Macros for getting elapsed time
  #define numberOfSeconds(_time_) (_time_ % SECS_PER_MIN)
  #define numberOfMinutes(_time_) ((_time_ / SECS_PER_MIN) % SECS_PER_MIN)
  #define numberOfHours(_time_) (( _time_% SECS_PER_DAY) / SECS_PER_HOUR)
  #define dayOfWeek(_time_)  ((( _time_ / SECS_PER_DAY + 4)  % DAYS_PER_WEEK)+1) // 1 = Sunday
  #define elapsedDays(_time_) ( _time_ / SECS_PER_DAY)  // this is number of days since Jan 1 1970
  #define elapsedSecsToday(_time_)  (_time_ % SECS_PER_DAY)   // the number of seconds since last midnight
  Leap Years
  1988
  1992
  1996
  2000
  2004
  2008
  2012
  2016
  2020
 */

  ageTime = currentTime - birthdayTime;
  breakTime(ageTime, ageElem);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Derek's Age"));
  // ageTime / SECS_PER_YEAR
  lcd.setCursor(0, 1);


  switch (function) {
    case 0:
      // Display age in Years
      lcd.print(ageElem.Year);
      lcd.print(' ');
      lcd.print(F("Years"));
      function = 1;
      break;
    case 1:
      lcd.print(ageTime / SECS_PER_WEEK);
      lcd.print(' ');
      lcd.print(F("Weeks"));
      function = 2;
      break;
    case 2:
      lcd.print(ageTime / SECS_PER_DAY);
      lcd.print(' ');
      lcd.print(F("Days"));
      function = 3;
      break;
    case 3:
      lcd.print(ageTime / SECS_PER_HOUR);
      lcd.print(' ');
      lcd.print(F("Hours"));
      function = 0;
      break;
    default:
      function = 0;
      break;
  }
}

void displayTemperature() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Temperature"));
  lcd.setCursor(0, 1);
  float c = tempsensor.readTempC();
  float f = c * 9.0 / 5.0 + 32;
  lcd.print(f, 2);

  // print the custom char to the lcd
  lcd.write((uint8_t)0);
  lcd.print('F');
  // why typecast? see: http://arduino.cc/forum/index.php?topic=74666.0
}

void displayLeadingCharacter(int number, char character) {
  if (number < 10)
    lcd.print(character);
  lcd.print(number);
}

void displayLeadingZero(int number){
  if(number < 10)
    lcd.print('0');
  lcd.print(number);
}

time_t getLocalTime(time_t utc, int TZ, boolean isDst) {
    // time_t here is a 32bit integer
    time_t localTime = utc + (TZ * SECS_PER_HOUR);

    if (isDst) {
      localTime += SECS_PER_HOUR;
    }

    return localTime;
}