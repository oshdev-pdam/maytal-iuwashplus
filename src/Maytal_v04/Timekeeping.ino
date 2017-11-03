static void rtcIRQ()
{
  RTC.alarm(ALARM_1);
}

void setRTCInterrupt()
{
  //  Set up the RTC interrupts on every minute at the 00 second mark
  RTC.squareWave(SQWAVE_NONE);
  RTC.setAlarm(ALM1_MATCH_SECONDS, 0, 0, 0, 1);
  RTC.alarm(ALARM_1);
  RTC.alarmInterrupt(ALARM_1, true);
}

void clockSet()
{
  char method;
  int netOffset;
  char theDate[17];

  wait(1000);    //  Give time for any trailing data to come in from FONA
  Serial.println(F("Attempting to get time from GSM location service..."));

  flushFona();    //  Flush any trailing data
  fona.sendCheckReply("AT+CIPGSMLOC=2,1", "OK");    //  Query GSM location service for time

  fona.parseInt();                    //  Ignore first int
  int secondInt = fona.parseInt();    //  Ignore second int -- necessary on some networks/towers
  int netYear = fona.parseInt();      //  Get the results -- GSMLOC year is 4-digit
  int netMonth = fona.parseInt();
  int netDay = fona.parseInt();
  int netHour = fona.parseInt();      //  GSMLOC is _supposed_ to get UTC; we will adjust
  int netMinute = fona.parseInt();
  int netSecond = fona.parseInt();    //  Our seconds may lag slightly, of course

  if (netYear < 2016 || netYear > 2050 || netHour > 23) //  If that obviously didn't work...
  {
    netSecond = netMinute;  //  ...shift everything up one to capture that second int
    netMinute = netHour;
    netHour = netDay;
    netDay = netMonth;
    netMonth = netYear;
    netYear = secondInt;

    Serial.println(F("Recombobulating..."));
  }

  if (netYear < 2016 || netYear > 2050 || netHour > 23)  // If that still didn't work...
  {
    Serial.println(F("GSM location service failed."));
    /*   ...the we'll get time from the NTP pool instead:
        (https://en.wikipedia.org/wiki/Network_Time_Protocol)
    */
    //fona.enableNTPTimeSync(true, "0.daimakerlab.pool.ntp.org");        //will change to NIST NTP server for obvious reasons
    fona.enableNTPTimeSync(true, (FONAFlashStringPtr) "time.nist.gov");
    Serial.println(F("Attempting to enable NTP sync."));

    wait(15000);                 // Wait for NTP server response

    fona.println(F("AT+CCLK?")); // Query FONA's clock for resulting NTP time
    netYear = fona.parseInt();    // Capture the results
    netMonth = fona.parseInt();
    netDay = fona.parseInt();
    netHour = fona.parseInt();    // We asked NTP for UTC and will adjust below
    netMinute = fona.parseInt();
    netSecond = fona.parseInt();  // Our seconds may lag slightly

    method = 'N';
  }
  else
    method = 'G';

  if ((netYear < 1000 && netYear >= 16 && netYear < 50) || (netYear > 1000 && netYear >= 2016 && netYear < 2050))
    //  If we got something that looks like a valid date...
  {
    //  Adjust UTC to local time
    if ((netHour + UTCOFFSET) < 0)                  //  If our offset + the UTC hour < 0...
    {
      netHour = (24 + netHour + UTCOFFSET);   //  ...add 24...
      netDay = (netDay - 1);                  //  ...and adjust the date to UTC - 1
    }
    else
    {
      if ((netHour + UTCOFFSET) > 23)         //  If our offset + the UTC hour > 23...
      {
        netHour = (netHour + UTCOFFSET - 24); //  ...subtract 24...
        netDay = (netDay + 1);                //  ...and adjust the date to UTC + 1
      }
      else
        netHour = (netHour + UTCOFFSET);      //  Otherwise it's straight addition
    }

    Serial.print(F("Obtained current time: "));
    sprintf(theDate, "%d/%d/%d %d:%d", netDay, netMonth, netYear, netHour, netMinute);
    Serial.println(theDate);

    Serial.println(F("Adjusting RTC."));

    setTime(netHour, netMinute, netSecond, netDay, netMonth, netYear);    //set the system time to 23h31m30s on 13Feb2009
    RTC.set(now());                                                       //set the RTC from the system time
  }
  
  else
  {
    Serial.println(F("Didn't find reliable time. Will continue to use RTC's current time."));
    method = 'X';
  }

  wait(200);              //  Give FONA a moment to catch its breath
}
