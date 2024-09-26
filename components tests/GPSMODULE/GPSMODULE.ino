void setup() {
  Serial.begin(9600);   // USB Serial for monitoring
  Serial1.begin(9600);  // Serial1 for communication with SIM800L

  // Send initial AT commands to the SIM800L
  sendATCommand("AT");              // Basic AT command to check communication
  sendATCommand("AT+CGPSPWR=1");    // Power on the GPS
  delay(2000);                      // Wait for GPS to power up
}

void loop() {
  // Request GPS location from the SIM800L
  sendATCommand("AT+CGPSINF=0");
  delay(5000); // Wait for GPS data to be ready
}

void sendATCommand(String command) {
  Serial1.println(command);  // Send command to SIM800L
  delay(1000);               // Wait for a response

  String response = "";  // Buffer to hold the response

  // Read and store the response
  while (Serial1.available()) {
    response += (char)Serial1.read();
  }

  // If the response contains GPS data, process it
  if (response.startsWith("+CGPSINF")) {
    processGPSData(response);
  }
}

void processGPSData(String gpsData) {
  // Example response format: +CGPSINF: 0,4124.8963,N,08151.6838,W,225444.000,130417,003.9,000.0,1

  // Split the response data using commas
  int latIndex = gpsData.indexOf(',') + 1;
  String latStr = gpsData.substring(latIndex, gpsData.indexOf(',', latIndex));
  
  int latDirIndex = gpsData.indexOf(',', latIndex) + 1;
  char latDir = gpsData.charAt(latDirIndex);
  
  int lonIndex = gpsData.indexOf(',', latDirIndex) + 1;
  String lonStr = gpsData.substring(lonIndex, gpsData.indexOf(',', lonIndex));
  
  int lonDirIndex = gpsData.indexOf(',', lonIndex) + 1;
  char lonDir = gpsData.charAt(lonDirIndex);

  // Convert to decimal format
  float latitude = convertToDecimal(latStr.toFloat(), latDir);
  float longitude = convertToDecimal(lonStr.toFloat(), lonDir);

  // Output to Serial Monitor: Latitude, Longitude, and Google Maps Link
  Serial.print("Lat: ");
  Serial.println(latitude, 6);
  Serial.print("Lon: ");
  Serial.println(longitude, 6);

  Serial.print("Google Maps: https://www.google.com/maps?q=");
  Serial.print(latitude, 6);
  Serial.print(",");
  Serial.println(longitude, 6);
}

float convertToDecimal(float coord, char dir) {
  int degrees = int(coord / 100);
  float minutes = coord - (degrees * 100);
  float decimal = degrees + (minutes / 60.0);

  // Apply direction
  if (dir == 'S' || dir == 'W') {
    decimal = -decimal;
  }

  return decimal;
}