// Use Serial1 instead of SoftwareSerial
void setup()
{
  // Begin serial communication with Arduino IDE (Serial Monitor)
  Serial.begin(9600);

  // Begin serial communication with SIM800L using Serial1
  Serial1.begin(9600);

  Serial1.println("Initializing..."); 
  delay(1000); // 

  Serial1.println("AT"); // Once the handshake test is successful, it will return OK
  updateSerial();

  Serial1.println("ATD+ +27633274367;"); // Replace ZZ with country code and xxxxxxxxxxx with phone number to dial
  updateSerial();
  delay(20000); // Wait for 20 seconds...
  Serial1.println("ATH"); // Hang up
  updateSerial();
}

void loop()
{
}

void updateSerial()
{
  delay(500);
  while (Serial.available()) 
  {
    Serial1.write(Serial.read()); // Forward what Serial received to Serial1
  }
  while (Serial1.available()) 
  {
    Serial.write(Serial1.read()); // Forward what Serial1 received to Serial
  }
}
