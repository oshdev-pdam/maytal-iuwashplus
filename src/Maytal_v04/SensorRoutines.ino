int readPressure()
{
  digitalWrite(Q1, HIGH);     //  Switch on transducer
  wait(3000);                 //  Let transducer settle

  float pressure = 0.00;
  int rawPressure = analogRead(PRESSURE_PIN);

  //  Validate that analogRead is between 0.5v-4.5v range of transducer
  if (rawPressure < 102 || rawPressure > 922)
  {
    pressure = 0;
  }
  else
  {
    pressure = ((rawPressure - 102.4) / 4.708);  //  Convert to PSI
  }

  digitalWrite(Q1, LOW);      //  Switch off transducer to save power

  return (int)pressure + 10;       //  Offset by +10 PSI
}
