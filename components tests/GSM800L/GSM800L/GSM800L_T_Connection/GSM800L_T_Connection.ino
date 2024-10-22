#define SerialMon Serial  // Use the default USB serial for monitoring
#define SerialSIM Serial1 // Use Serial1 for SIM800L

void setup()
{
  // Begin communication with Serial Monitor
  SerialMon.begin(9600);

  // Begin communication with SIM800L on Serial1
  SerialSIM.begin(9600);
  
  SerialMon.println("Initializing...");
  delay(1000);

  SerialSIM.println("AT"); // Handshake test
  updateSerial();
  
  SerialSIM.println("AT+CSQ"); // Signal quality test
  updateSerial();
  
  SerialSIM.println("AT+CCID"); // Read SIM info
  updateSerial();
  
  SerialSIM.println("AT+CREG?"); // Check network registration
  updateSerial();

}

void loop()
{
  updateSerial();
}

void updateSerial()
{
  delay(500);
  
  // Forward data from Serial Monitor to SIM800L
  while (SerialMon.available())
  {
    SerialSIM.write(SerialMon.read());
  }
  
  // Forward data from SIM800L to Serial Monitor
  while (SerialSIM.available())
  {
    SerialMon.write(SerialSIM.read());
  }
}
