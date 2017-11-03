void flushFona()
{
  // Read all available serial input from FONA to flush any pending data.
  while (fona.available())
  {
    char c = fona.read();
    Serial.print(c);
  }
}

boolean sendPressure(short pressure)
{
  fonaOn();

  Serial.println(F("Sending pressure reading..."));

  char url[255];

  unsigned int voltage;
  fona.getBattVoltage(&voltage);              //  Read the battery voltage from FONA's ADC

  //  Convert PSI to bar
  float bar = pressure * 0.069;
  int whole = bar;
  int remainder = (bar - whole) * 1000;       //WHAT??????

  boolean success = false;
  int attempts = 0;

  wait(7500);                                 //  A long delay here seems to improve reliability

  while (success == false && attempts < 5)  //  We'll attempt up to five times to upload data
  {
    if (postRequest(1, bar, remainder, currentTime) == false) 
      success = false;
          
    else 
    {
      if (postRequest(2, pressure, 0, currentTime) == false) 
        success = false;
      
      else 
      {
        if (postRequest(3, voltage, 0, currentTime) == false) 
          success = false;
        
        else 
          success = true;
      }
    }
    attempts++;
  }

  //lastPressure = pressure;        //needs readjusting to per-minute data array.
  fonaOff();

  return success;
}

void fonaOff()
{
  wait(5000);        //  Shorter delays yield unpredictable results

  //  We'll turn GPRS off first, just to have an orderly shutdown
  if (fona.enableGPRS(false) == false)
  {
    if (fona.GPRSstate() == 1)
      Serial.println(F("Failed to turn GPRS off."));
    else
      Serial.println(F("GPRS is off."));
  }

  wait(500);

  // Power down the FONA if it needs it
  if (digitalRead(FONA_PS) == HIGH)     //  If the FONA is on...
  {
    fona.sendCheckReply("AT+CPOWD=1", (FONASTR) "OK");    //  ...send shutdown command...
    digitalWrite(FONA_KEY, HIGH);                         //  ...and set Key high
  }
}

boolean fonaOn()
{
  if (digitalRead(FONA_PS) == LOW)                //  If the FONA is off...
  {
    Serial.print(F("Powering FONA on..."));
    while (digitalRead(FONA_PS) == LOW)
    {
      digitalWrite(FONA_KEY, LOW);                //  ...pulse the Key pin low...
      wait(500);
    }
    digitalWrite(FONA_KEY, HIGH);                 //  ...and then return it to high
    Serial.println(F(" done."));
  }

  Serial.println(F("Initializing FONA..."));

  fonaSerial.begin(4800);                      //  Open a serial interface to FONA

  if (fona.begin(fonaSerial) == false)        //  Start the FONA on serial interface
  {
    Serial.println(F("FONA not found. Check wiring and power."));
    return false;
  }
  
  else
  {
    Serial.print(F("FONA online. "));

    unsigned long gsmTimeout = millis() + 30000;
    boolean gsmTimedOut = false;

    Serial.print(F("Waiting for GSM network... "));
    
    while (1)                             //This infinite loop probably was the culprit for 23 Oct freezing. Just my $0.02
    {
      byte network_status = fona.getNetworkStatus();
      if (network_status == 1 || network_status == 5) 
        break;

      if (millis() >= gsmTimeout)         //calling millis() again simultaneously with wait() which uses millis() too wasn't a good idea 
      {
        gsmTimedOut = true; 
        break;
      }
      wait(250);
    }

    if (gsmTimedOut == true)
    {
      Serial.println(F("timed out. Check SIM card, antenna, and signal."));
      return false;
    }
    
    else
      Serial.println(F("done."));

    //  RSSI is a measure of signal strength -- higher is better; less than 10 is worrying
    byte rssi = fona.getRSSI();
    Serial.print(F("RSSI: "));
    Serial.println(rssi);

    wait(3000);    //  Give the network a moment

    //fona.setGPRSNetworkSettings(F("internet"));    //  Set APN to your local carrier

    if (rssi > 5)
    {
      if (fona.enableGPRS(true) == false);
      {
        //  Sometimes enableGPRS() returns false even though it succeeded
        if (fona.GPRSstate() != 1)
        {
          for (byte GPRSattempts = 0; GPRSattempts < 5; GPRSattempts++)
          {
            Serial.println(F("Trying again..."));
            wait(2000);
            fona.enableGPRS(true);

            if (fona.GPRSstate() == 1)
            {
              Serial.println(F("GPRS is on."));
              break;
            }
            else
              Serial.print(F("Failed to turn GPRS on... "));
          }
        }
      }
    }
    
    else
    {
      Serial.println(F("Can't transmit, network signal strength is poor."));
      gsmTimedOut = true;
    }

    return true;
  }
}

boolean postRequest(int feed, int value, int remainder, time_t epoch) {
  String feedString;

  switch (feed) {
    case 1: {
        feedString = "bar";
        break;
      }
    case 2: {
        feedString = "psi";
        break;
      }
    case 3: {
        feedString = "voltage";
        break;
      }
    default: {
        return false;
      }
      break;
  }

  //  Manually construct the HTTP POST headers necessary to send the data to the feed
  fona.sendCheckReply("AT+HTTPINIT", (FONASTR) "OK");
  fona.sendCheckReply("AT+HTTPPARA=\"CID\",1", (FONASTR) "OK");
  fona.print(F("AT+HTTPPARA=\"URL\",\"io.adafruit.com/api/v2/iqnaul/feeds/"));
  fona.print(feedString);
  fona.println(F("/data\""));
  fona.expectReply((FONASTR) "OK");
  fona.sendCheckReply("AT+HTTPPARA=\"REDIR\",\"0\"", (FONASTR) "OK");
  fona.sendCheckReply("AT+HTTPPARA=\"CONTENT\",\"application/json\"", (FONASTR) "OK");
  fona.print(F("AT+HTTPPARA=\"USERDATA\",\"X-AIO-KEY: "));
  fona.print(AIO_KEY);
  fona.println(F("\""));
  Serial.print(F("AT+HTTPPARA=\"USERDATA\",\"X-AIO-KEY: "));
  Serial.print(F("XXXXXXXXXXXXXXXX"));
  Serial.println(F("\""));
  fona.expectReply((FONAFlashStringPtr)"OK");
  //fona.sendCheckReply(F("AT+HTTPSSL=1"), F("OK"));

  //  For debugging
  Serial.print(F("Posting to URL: "));
  Serial.print(F("\"io.adafruit.com/api/v2/iqnaul/feeds/"));
  Serial.print(feedString);
  Serial.println(F("/data\""));

  char json[64];

  /*if(remainder == 0) {
    sprintf(json, "{\"value\": %d, \"\created_epoch\": %s}", value, (char)epoch);
    } else {
    sprintf(json, "{\"value\": %d.%d, \"\created_epoch\": %s}", value, remainder, (char)epoch);
    }*/

  if (remainder == 0)
    sprintf(json, "{\"value\": %d}", value);
  else
    sprintf(json, "{\"value\": %d.%d}", value, remainder);

  int dataSize = strlen(json);

  fona.print (F("AT+HTTPDATA="));
  fona.print (dataSize);
  fona.println (F(",2000"));
  fona.expectReply ((FONASTR) "OK");

  fona.sendCheckReply(json, (FONASTR) "OK");

  Serial.println(json);

  //  For debugging
  Serial.print (F("AT+HTTPDATA="));
  Serial.print (dataSize);
  Serial.println (F(",2000"));

  uint16_t statusCode;
  uint16_t dataLen;

  fona.HTTP_action(1, &statusCode, &dataLen, 30000);   //  Send the POST request we've constructed

  while (dataLen > 0)
  {
    while (fona.available())
    {
      char c = fona.read();
      loop_until_bit_is_set (UCSR0A, UDRE0);
      UDR0 = c;
    }

    dataLen--;
    if (!dataLen)
    {
      break;
    }
  }

  Serial.print (F("Status code: "));
  Serial.println (statusCode);
  Serial.print (F("Reply length: "));
  Serial.println (dataLen);

  fona.HTTP_POST_end();

  if (statusCode == 200)
    return true;
  else
    return false;
}
