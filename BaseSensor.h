#ifndef BaseSensor_h
#define BaseSensor_h

#include "Arduino.h"
#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#warning "need to implement eeprom"
#define EEPROM_WRITE_DWORD(a,b) eeWriteInt((int)a,b)
#define EEPROM_READ_DWORD(a) eeGetInt((int)a)
#define EEPROM_READ_BYTE(a) EEPROM.read((int)a)
#define EEPROM_WRITE_BYTE(a,b) EEPROM.write((int)a,b)
#define ETHERNETCLIENT WiFiClient
#else
#include <avr/eeprom.h>
#include <EthernetClient.h>
#define EEPROM_WRITE_DWORD(a,b) eeprom_write_dword(a,b)
#define EEPROM_READ_DWORD(a) eeprom_read_dword(a)
#define EEPROM_READ_BYTE(a) eeprom_read_byte(a)
#define EEPROM_WRITE_BYTE(a,b) eeprom_write_byte(a,b)
#define ETHERNETCLIENT EthernetClient
#endif

#include "FlashMini.h"
#include <SoftwareSerial.h>

#ifdef ESP8266
static inline unsigned int eeGetInt(int pos) {
  unsigned int val;
  byte* p = (byte*) &val;

  *p        = EEPROM.read(pos);
  *(p + 1)  = EEPROM.read(pos + 1);
  *(p + 2)  = EEPROM.read(pos + 2);
  *(p + 3)  = EEPROM.read(pos + 3);
  return val;
}

static inline void eeWriteInt(int pos, unsigned int val) {
    byte* p = (byte*) &val;

    EEPROM.write(pos, *p);
    EEPROM.write(pos + 1, *(p + 1));
    EEPROM.write(pos + 2, *(p + 2));
    EEPROM.write(pos + 3, *(p + 3));
    EEPROM.commit();
}
#endif

class BaseSensor
{
  public:
  BaseSensor(int ppu, unsigned int sid, int f);// constructor
  virtual void Begin(byte index);              // initialize all variables
  virtual void CheckSensor();                  // check the input and update counters (called from 5ms interrupt)
  virtual void Loop(int m);                    // Called from main loop				
  virtual void CalculateActuals();             // Convert all counters according to the pulses per unit
  virtual void Status(Print& client);          // Dump status to ethernet
  virtual void Reset();                        // Reset day-counter
  virtual void Save();                         // Save day-counter
  virtual void Update(long Value);             // Update and save the day counter
  virtual void NewTotal(long value);           // Update and save the total counter
  virtual void ResetPeak();                    // reset peak so new peak measurement can start
  long Midnight;                               // The total counter value at the last midnight
  long Today;                                  // Total for today in correct units. Reset at midnight
  long Actual;                                 // Actual measured value in correct units
  long Peak;                                   // Peak value of the last period
  unsigned int SID;                            // System id where this sensor logs to
  byte Type;                                   // Variable of PvOutput to log to. See userdefs.h for explanation  
  int Factor;                                  // Values will be divided by this number before sending them to pvOutput
  
  protected:
  long todayCnt;                               // today's pulse count
  int  ppu;                                    // the pulses per unit (kWh or m3) for this counter
  long lastMillis;                             // the time of the last pulse (ms)
  long pulseLength;                            // the time between the last two pulses (ms)
  int ee;                                      // the address to store the day counter in case of a reset
  int ee2;                                     // the address to store the total counter in case of a reset
};

#endif

