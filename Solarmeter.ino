#define VERSION "V11.43"

#ifdef ESP8266
#include <Ticker.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
Ticker EspTimer;
WiFiUDP Udp;
WiFiServer server(555);
#else
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <MsTimer2.h>
#include <avr/wdt.h>
#include <utility/w5100.h>
EthernetUDP Udp;
EthernetServer server(555);  // port changed from 80 to 555
#endif
#include <TimeLib.h>

#include "FlashMini.h"
#include "S0Sensor.h"
#include "P1GasSensor.h"
#include "P1Power.h"
#include "AnalogSensor.h"
#include "FerrarisSensor.h"
#include "Temperature.h"
#include "userdefs.h"

#include <Dns.h>

// global variables
byte   lastDayReset;
byte   lastHour;
byte   lastMinute;
byte   iDay;
byte   iHour;
byte   iMinute;
int    upTime;               // the amount of hours the Arduino is running
char   webData[70];
#if defined(USE_LOGGING) && !defined(ESP8266)
#include <SD.h>
  File   logFile;
#endif
#define EE_RESETDAY 4

void setup()
{
    // initialize network
#ifdef ESP8266
    Serial.begin(115200);
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    EEPROM.begin(1024);
#else
    // wait for the ethernet shield to wakeup
    delay(300);
    Ethernet.begin(mac, ip, dnsserver, gateway, subnet);
    // set connect timeout parameters
    W5100.setRetransmissionTime(2000); // 200ms per try
    W5100.setRetransmissionCount(8);
#endif
    // Try to set the time 10 times
    UpdateTime();

    #ifdef USE_LOGGING
        // initialize SD card
        SetupSD();
        OpenLogFile();
    #endif
    // start listening
    server.begin();

    // initialize the sensors
    for(byte i = 0; i < NUMSENSORS; i++)
    {
        sensors[i]->Begin(i);
    }
    // set a random seed
    randomSeed(analogRead(0));

    // restore the last day on which the counters were reset
    lastDayReset = EEPROM_READ_BYTE((uint8_t*) EE_RESETDAY);
    // if the eeprom contains illegal data, set it to a useful value
    if(lastDayReset == 0 || lastDayReset > 31) lastDayReset = day();
    lastMinute = minute();
    lastHour = hour();
    upTime = 0;

    #ifdef USE_WD
      SetupWatchdog();
    #endif
    // start the timer interrupt
#ifdef ESP8266
    EspTimer.attach_ms(5, Every5ms); //Use <strong>attach_ms</strong> if you need time in ms
#else
    MsTimer2::set(5, Every5ms); // 5ms period
    MsTimer2::start();
#endif
}

// check and update all counters every 5ms.
void Every5ms()
{
    for(byte i = 0; i < NUMSENSORS; i++)
    {
        sensors[i]->CheckSensor();
    }
    #ifdef USE_WD
      CheckWatchdog();
    #endif
}

void loop()
{
    // get the actual time
    iDay = day();
    iHour = hour();
    iMinute = minute();
    // reset counters when todays day is different from the last day the counters were reset
    if(iDay != lastDayReset)
    {
        busy(1);
        #ifdef USE_MINDERGAS
            // Calculate the new gas metervalue and start the countdown
            UpdateGas();
        #endif
        for(byte i = 0; i < NUMSENSORS; i++)
        {
            sensors[i]->Reset();
        }
        #ifdef USE_LOGGING
            // create new logfile
            CloseLogFile();
            OpenLogFile();
        #endif
        lastDayReset = iDay;
        // store today as the date of the last counter reset
        EEPROM_WRITE_BYTE((uint8_t*) EE_RESETDAY, lastDayReset);
    }

    // hour has changed
    // cannot simply check the change of an hour because 'updatetime' can also change the hour
    // therefore we also check that the minutes are 0
    if(iHour != lastHour && iMinute == 0)
    {
        busy(2);
        lastHour = iHour;
        upTime++;
        // save the daily values every hour
        for(byte i = 0; i < NUMSENSORS; i++)
        {
            sensors[i]->Save();
        }
        // sync the time at fixed interval
        if(lastHour == 2 || lastHour == 14)
        {
            UpdateTime();
        }
        #ifdef USE_MAIL
            if(lastHour == MAIL_TIME)
            {
                SendMail();
            }
        #endif
    }

    // update every minute
    if(iMinute != lastMinute)
    {
        busy(3);
        lastMinute = iMinute;
        for(byte i = 0; i < NUMSENSORS; i++)
        {
            sensors[i]->CalculateActuals();
        }
        busy(31);

        #ifdef USE_MINDERGAS
            // this function will not do anything until the countdown timer is finished
            SendToMinderGas();
        #endif

        #ifdef USE_LOGGING
            WriteDateToLog();
            for(byte i = 0; i < NUMSENSORS; i++)
            {
                //sensors[i]->Status(&logFile);
#ifdef ESP8266
                Serial.print(sensors[i]->Today); Serial.print(";"); Serial.print(sensors[i]->Actual); Serial.println(";");
#else
                logFile << sensors[i]->Today << ";" << sensors[i]->Actual << ";" << endl;
#endif
            }
#ifndef ESP8266
            logFile << endl;
            logFile.flush();
#endif
        #endif
        busy(32);
        // update every 5 minutes or whatever is set in userdefs
        if((lastMinute%UPDATEINTERVAL) == 0)
        {
            SendToPvOutput(sensors);
            busy(33);
            // reset the maximum for pvoutput
            for(byte i = 0; i < NUMSENSORS; i++)
            {
                sensors[i]->ResetPeak();
            }
        }
        busy(34);
        #ifdef EXOSITE_KEY
          if((lastMinute%EXOSITEUPDATEINTERVAL) == 0)
          {
            SendToExosite();
          }
        #endif
    }
    busy(4);
    // let all sensors do other stuff
    for(byte i = 0; i < NUMSENSORS; i++)
    {
      sensors[i]->Loop(lastMinute);
    }
    busy(5);
    // see if there are clients to serve
    ServeWebClients();
    busy(0);
    // give the ethernet shield some time to rest
    delay(50);
}

