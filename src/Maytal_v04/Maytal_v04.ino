#define VERSION       "0.04"
#define UTCOFFSET     +7          //  Local standard time variance from UTC
#define INTERVAL      5           //  Number of minutes between readings
#define IO_USERNAME   "iqnaul"
#define AIO_KEY       "1c58536b835f49aab1812ca1bb164bea"  //  Adafruit IO key
#define FONASTR       FONAFlashStringPtr

//- - - - - - - - - - - - - - - - - - - - - -

#include <Adafruit_FONA.h>    //  https://github.com/adafruit/Adafruit_FONA
#include <DS3232RTC.h>        //  https://github.com/JChristensen/DS3232RTC
#include <Sleep_n0m1.h>       //  https://github.com/n0m1/Sleep_n0m1
#include <SoftwareSerial.h>   //  Standard software serial library
#include <TimeLib.h>          //  Standard time library
#include <Wire.h>             //  Standard wire library for comms with RTC

#define PRESSURE_PIN A0       //  Pressure reading pin from transducer
#define RTC_INTERRUPT_PIN 2   //  Square wave pin from RTC
#define Q1 3                  //  Base pin for transducer switch
#define FONA_RST 5            //  FONA reset pin
#define FONA_RX 6             //  UART pin to FONA
#define FONA_TX 7             //  UART pin from FONA
#define FONA_KEY 8            //  FONA Key pin
#define FONA_PS 9             //  FONA power status pin

time_t  currentTime;
time_t  dataTimestamps[INTERVAL];
short   pressureData[INTERVAL];
short   dataIndex = 0;

boolean sentData = false;

Adafruit_FONA fona = Adafruit_FONA (FONA_RST);
SoftwareSerial fonaSerial = SoftwareSerial (FONA_TX, FONA_RX);

Sleep sleep;

void setup()
{
  pinMode(RTC_INTERRUPT_PIN, INPUT_PULLUP);
  pinMode(FONA_KEY, OUTPUT);
  pinMode(FONA_RX, OUTPUT);
  pinMode(Q1, OUTPUT);

  Serial.begin(57600);
  Serial.print(F("Maytal v"));
  Serial.println(VERSION);
  Serial.print(__DATE__);   //  Compile data and time helps identify software uploads
  Serial.print(F(" "));
  Serial.println(__TIME__);

  setRTCInterrupt();

  //attach RTC 1-minute pulse interrupt then enable all-interrupts (#asm("sei")) 
  attachInterrupt(digitalPinToInterrupt(RTC_INTERRUPT_PIN), rtcIRQ, FALLING);
  interrupts();

  digitalWrite(Q1, LOW);    //  Turn off transducer to save power

  fonaOn();
  clockSet();

  //Delete any accumulated SMS messages to avoid interference from old commands
  //fona.sendCheckReply (F("AT+CMGF=1"), F("OK"));            //  Enter text mode
  //fona.sendCheckReply (F("AT+CMGDA=\"DEL ALL\""), F("OK")); //  Delete all SMS messages

  fonaOff();
  wait(2000);
}

void loop()
{
  currentTime = RTC.get();

  Serial.print(hour(currentTime));
  Serial.print(F(":"));
  
  if (minute(currentTime) < 10)
    Serial.print(F("0"));               //clock left zero padding
  Serial.println(minute(currentTime));

  if (dataIndex < INTERVAL-1 )          //if data buffer hasn't filled yet
  {
    dataTimestamps[dataIndex] = currentTime;
    pressureData[dataIndex] = readPressure();
    dataIndex++;
  }

  else if (dataIndex == INTERVAL - 1)     //safeguard
  {
    short x;
    for (x = 0; x < 5; x++)
    {
      sendPressure(pressureData[x]);
    }
  }

  Serial.print(pressureData[dataIndex]);
  Serial.println(F(" PSI."));

  Serial.flush();                       //  Flush any serial output before sleep
  RTC.setAlarm(ALM1_MATCH_SECONDS, 0, 0, 0, 1);
  RTC.alarm(ALARM_1);                   //  Clear any outstanding RTC alarms
  RTC.alarmInterrupt(ALARM_1, true);
  sleep.pwrDownMode();
  wait(500);
  sleep.sleepInterrupt(digitalPinToInterrupt(RTC_INTERRUPT_PIN), FALLING); //  Sleep; wake on falling voltage on RTC pin
}

void wait(unsigned int ms)  //  Non-blocking delay function (STILL HIGHLY UNDER-UTILIZED)
{
  unsigned long period = millis() + ms;
  while (millis() < period)
  {
    Serial.flush();
  }
}

/*  DEPRECATED: 10% filter to save power and data sending status return mechanism
//  If pressure has changed more than 10% since last upload, upload new reading
if (((lastPressure * 100) - (currentPressure * 100)) / currentPressure >= 10)
{
sendPressure(currentPressure);
}

//  If it is time to send a scheduled reading, send it
if (minute(currentTime) % INTERVAL == 0 && sentData == false)
{
if (sendPressure(currentPressure) == true)
sentData = true;
else
sentData = false;
}
//else                            //probably misplaced else clause
//  sentData = false;
*/