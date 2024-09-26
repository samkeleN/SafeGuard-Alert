// Use Serial1 instead of SoftwareSerial

void setup()
{
  // Begin serial communication with Arduino IDE (Serial Monitor)
  Serial.begin(9600);
  
  // Begin serial communication with SIM800L using Serial1
  Serial1.begin(9600);

  Serial.println("Initializing..."); 
  delay(1000);

  // Send AT command to check connection
  Serial1.println("AT"); // Once the handshake test is successful, it will return OK
  updateSerial();

  // Set SMS mode to TEXT
  Serial1.println("AT+CMGF=1"); // Configuring TEXT mode
  updateSerial();

  // Send SMS command with phone number
  Serial1.println("AT+CMGS=\"+27633274367\""); // Replace ZZ with country code and xxxxxxxxxxx with phone number
  updateSerial();

  // SMS content
  Serial1.print("Samkele needs help! |Here is his location: https://github.com/samkeleN");
  updateSerial();

  // End SMS with Ctrl+Z (ASCII code 26)
  Serial1.write(26);
}

void loop()
{
}

void updateSerial()
{
  delay(500);

  // Forward data from Serial Monitor to Serial1 (SIM800L)
  while (Serial.available()) 
  {
    Serial1.write(Serial.read()); 
  }

  // Forward data from Serial1 (SIM800L) to Serial Monitor
  while (Serial1.available()) 
  {
    Serial.write(Serial1.read()); 
  }
}
