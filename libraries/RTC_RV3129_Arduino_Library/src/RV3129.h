/******************************************************************************
<Original Header Comments>
SparkFun_RV1805.h
RV1805 Arduino Library
Andy England @ SparkFun Electronics
February 5, 2018
https://github.com/sparkfun/Qwiic_RTC

Resources:
Uses Wire.h for i2c operation
Uses SPI.h for SPI operation

Development environment specifics:
Arduino IDE 1.6.4

This code is released under the [MIT License](http://opensource.org/licenses/MIT).
Please review the LICENSE.md file included with this example. If you have any questions 
or concerns with licensing, please contact techsupport@sparkfun.com.
Distributed as-is; no warranty is given.
******************************************************************************/


/*
TP-3000 PATCH NOTE 2026-06-21 / S. Brachtl (DK6WT)

This bundled RV3129 library copy contains TP-3000-specific RTC fixes for
stable 24-hour operation:
- begin() must not force the RTC into 12-hour mode.
- setTime() writes the hour register explicitly in 24-hour mode.
- set24Hour() masks the RV3129 hour control bits before BCD conversion.
- 12 AM is converted to 00, and 12 PM remains 12.
- updateTime() normalizes a 12-hour RTC register into a 0..23 hour value for
  the TP-3000 application.

Reason: the original automatic 12h/24h switching could corrupt the hour around
00:xx / 12:xx in TP-3000 long-term logging. The original MIT license and
upstream attribution remain unchanged.
*/

#pragma once

#if (ARDUINO >= 100)
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include <Wire.h>

//The 7-bit I2C address of the RV3129
#define RV3129_ADDR						(uint8_t)0x56

/**************************
* Registers (RV-3129)
***************************/
// Control Page
#define RV3129_CTRL_1					0x00
#define RV3129_CTRL_INT					0x01
#define RV3129_CTRL_INT_FLAG			0x02
#define RV3129_CTRL_STATUS				0x03
#define RV3129_CTRL_RESET				0x04

// Clock Page
#define RV3129_SECONDS      			0x08
#define RV3129_MINUTES      			0x09
#define RV3129_HOURS        			0x0A
#define RV3129_DATE         			0x0B // Days?
#define RV3129_WEEKDAYS      			0x0C
#define RV3129_MONTHS        			0x0D
#define RV3129_YEARS        			0x0E

#define HOURS_12_24						6 // 6th bit in HOURS register determines 12h(1) vs 24h(0) mode
#define HOURS_AM_PM						5 // 5th bit in HOURS register -> AM(0) vs PM(1) in 12h mode

// Alarm Page
#define RV3129_SECONDS_ALM    			0x10
#define RV3129_MINUTES_ALM     			0x11
#define RV3129_HOURS_ALM       			0x12
#define RV3129_DATE_ALM        			0x13
#define RV3129_WEEKDAYS_ALM    			0x14
#define RV3129_MONTHS_ALM      			0x15
#define RV3129_YEARS_ALM    			0x16 // (This was not in RV-1805)

// Timer Page
#define RV3129_TIME_LOW					0x18
#define RV3129_TIME_HIGH				0x19

// Temperature Page
#define RV3129_TEMP						0x20

// RAM Page
#define RV3129_User_RAM					0x38

/**************************
* END of Registers (RV-3129)
***************************/

#define TIME_ARRAY_LENGTH 7 // Total number of writable values in device
#define ALARM_ARRAY_LENGTH 7

enum time_order {
	TIME_SECONDS,    // 0
	TIME_MINUTES,    // 1
	TIME_HOURS,      // 2
	TIME_DATE,       // 3
	TIME_DAY,		 // 4
	TIME_MONTH,      // 5
	TIME_YEAR		 // 6
};

class RV3129
{
  public:
	
    RV3129( void );

    boolean begin( TwoWire &wirePort = Wire);
	void initMSG();

	bool setTime(uint8_t sec, uint8_t min, uint8_t hour, uint8_t date, uint8_t month, uint16_t year, uint8_t day);
	bool setTime(uint8_t * time, uint8_t len);
	bool setSeconds(uint8_t value);
	bool setMinutes(uint8_t value);
	bool setHours(uint8_t value);
	bool setWeekday(uint8_t value);
	bool setDate(uint8_t value);
	bool setMonth(uint8_t value);
	bool setYear(uint8_t value);
	
	bool updateTime(); //Update the local array with the RTC registers
	
	char* stringDateUSA(); //Return date in mm-dd-yyyy
	char* stringDate(); //Return date in dd-mm-yyyy
	char* stringTime(); //Return time hh:mm:ss with AM/PM if in 12 hour mode
	char* stringTimeStamp(); //Return timeStamp in ISO 8601 format yyyy-mm-ddThh:mm:ss
	char* stringTemp(); //Return temperature value in Deg. C
	
	uint8_t getSeconds();
	uint8_t getMinutes();
	uint8_t getHours();
	uint8_t getWeekday();
	uint8_t getDate();
	uint8_t getMonth();
	uint8_t getYear();	

	uint8_t getTemp();
	
	bool setToCompilerTime(); //Uses the hours, mins, etc from compile time to set RTC
	
	bool is12Hour(); //Returns true if 12hour bit is set
	bool isPM(); //Returns true if is12Hour and PM bit is set
	void set12Hour();
	void set24Hour();
	
	uint8_t status(); //Returns the status byte
	
	bool setAlarm(uint8_t sec, uint8_t min, uint8_t hour, uint8_t date, uint8_t week_day, uint8_t month, uint8_t year);
	bool setAlarm(uint8_t * time, uint8_t len);
	bool enableDisableAlarm(uint8_t enableBits); //bits 0 to 6, alarm goes off with match of second, minute, hour, etc
	uint8_t getAlarmMode(); //bits 0 to 6, alarm goes off with match of second, minute, hour, etc
							// (mode > 0x7f) => read registers error
	bool getAlarmFlag(); // 0: no alarm interrupt, 1: alarm interrupt generated when Time & Date matches Alarm setting
	bool alarmINTEnabled(); // 0: disabled, 1: enabled
	bool enableAlarmINT(bool enableINT);

	bool setTimer(uint16_t time16);
	bool getTimerFlag();
	bool timerINTEnabled();
	bool enableTimerINT(bool enableTimer);
	bool enableTimerAutoReload(bool enableTAR);
	bool timerAutoReloadEnabled();

	bool systemReset();

	bool enableWatch1HzClkSrc(bool enableWE);
	bool watch1HzClkSrcEnabled();

	bool enableAutomaticEEPROMRefresh(bool enableEERE);
	bool automaticEEPROMRefreshEnabled();

	bool enableSelfRecovery(bool enableSR);
	bool selfRecoveryEnabled();

	bool setCountdownTimerSource(uint8_t srcTD); // valid param value range: 0b00 to 0b11
	uint8_t getCountdownTimerSource(); // returns 0xFF on error. Valid return value range: 0b00 to 0b11

	bool setCLKOUTPinFunction(bool clkFoo); // 0: INT function on CLKOUT pin, 1: CLKOUT function on CLKOUT pin
	bool getCLKOUTPinFunction();

	uint8_t getCTRL1Register();
	bool setCTRL1Register(uint8_t ctrl);

	bool enableSelfRecoveryINT(bool enableSRINT);
	bool selfRecoveryINTEnabled();

	bool enableVLOW2INT(bool enableVLOW2);
	bool VLOW2INTEnabled();

	bool enableVLOW1INT(bool enableVLOW1);
	bool VLOW1INTEnabled();

	uint8_t getCTRLINTRegister();
	bool setCTRLINTRegister(uint8_t ctrlINT);

	bool getSelfRecoveryFlag();
	bool getVLOW2Flag();
	bool getVLOW1Flag();

	uint8_t getINTFlagRegister();

	uint8_t getCTRLStatusRegister();
	
	//Values in RTC are stored in Binary Coded Decimal. These functions convert to/from Decimal
	uint8_t BCDtoDEC(uint8_t val); 
	uint8_t DECtoBCD(uint8_t val);

	void reset(void); //Fully reset RTC to all zeroes
	
    uint8_t readRegister(uint8_t addr);
    bool writeRegister(uint8_t addr, uint8_t val);
	bool readMultipleRegisters(uint8_t addr, uint8_t * dest, uint8_t len);
	bool writeMultipleRegisters(uint8_t addr, uint8_t * values, uint8_t len);

private:
	uint8_t _time[TIME_ARRAY_LENGTH];
	TwoWire *_i2cPort;
};
