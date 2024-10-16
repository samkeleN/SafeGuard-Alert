#include <TinyGPS++.h>

static const uint32_t GPSBaud = 9600; // GPS module baud rate

TinyGPSPlus gps;

void setup()
{
  Serial.begin(9600);   // Initialize Serial for communication with the computer
  Serial1.begin(GPSBaud); // Initialize Serial1 for GPS module (RX on pin 0, TX on pin 1)
}

void loop()
{
  while (Serial1.available() > 0)
  {
    if (gps.encode(Serial1.read())) 
    {
      displayInfo(); // Display GPS data
    }
  }

  // Check if no GPS data is received after 5 seconds
  if (millis() > 5000 && gps.charsProcessed() < 10)
  {
    Serial.println(F("No GPS detected: check wiring."));
    while (true); // Stop execution if no GPS data is detected
  }
}

void displayInfo()
{
  Serial.print(F("Location: ")); 
  if (gps.location.isValid())
  {
    Serial.print(gps.location.lat(), 6);
    Serial.print(F(","));
    Serial.print(gps.location.lng(), 6);
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.print(F("  Date: "));
  if (gps.date.isValid())
  {
    Serial.print(gps.date.month());
    Serial.print(F("/"));
    Serial.print(gps.date.day());
    Serial.print(F("/"));
    Serial.print(gps.date.year());
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.println(); // Print a new line
}
