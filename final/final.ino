#include <Wire.h>
#include "I2Cdev.h"
#include "MPU6050.h"
#include <math.h>

#define SerialMon Serial   // Use default USB serial for monitoring
#define SerialSIM Serial1  // Use Serial1 for SIM800L

MPU6050 mpu;
int16_t ax, ay, az;
int16_t gx, gy, gz;

int deltx = 0, delty = 0, deltz = 0;
int vibration = 0, magnitude = 0;
int sensitivity = 45, devibrate = 75;
double angle;

unsigned long time1;
byte updateflag;

void setup() {
  SerialMon.begin(9600);   // Begin communication with Serial Monitor
  SerialSIM.begin(9600);   // Begin communication with SIM800L on Serial1

  Wire.begin();
  mpu.initialize();

  // Check MPU6050 connection
  if (mpu.testConnection()) {
    SerialMon.println("MPU6050 connection successful");
  } else {
    SerialMon.println("MPU6050 connection failed");
  }

  // Initialize SIM800L
  SerialMon.println("Initializing SIM800L...");
  delay(1000);

  // SIM800L setup
  SerialSIM.println("AT"); // Handshake test
  updateSerial();
  SerialSIM.println("AT+CSQ"); // Signal quality test
  updateSerial();
  SerialSIM.println("AT+CCID"); // Read SIM information
  updateSerial();
  SerialSIM.println("AT+CREG?"); // Check network registration
  updateSerial();

  time1 = micros();
}

void loop() {
  // Call impact routine every 2ms
  if (micros() - time1 > 1999) {
    Impact();
  }

  SerialMon.println(magnitude);
  // If the magnitude of the impact is greater than 3000, initiate call and send SMS
  if (magnitude > 3000) {
    // Display impact information if detected
    if (updateflag > 0) {
      updateflag = 0;
      SerialMon.print("Impact detected!! ");
      SerialMon.print("Magnitude: ");
      SerialMon.print(magnitude);
      SerialMon.print("\t Angle: ");
      SerialMon.println(angle, 2);
      makeCall();
      sendSMS();
      delay(2000);  // Optional: Delay to avoid repeated calls
    }
  }
}

void Impact() {
  time1 = micros(); // Reset time value

  static int oldx = 0, oldy = 0, oldz = 0; // Store previous axis readings for comparison

  // Read accelerometer data
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  // Convert raw data to a more readable range
  int xaxis = ax / 16384.0 * 1000; // assuming ±2g range
  int yaxis = ay / 16384.0 * 1000;
  int zaxis = az / 16384.0 * 1000;

  vibration--; // Loop counter prevents false triggering
  if (vibration < 0) vibration = 0;

  // Exit if in anti-vibration mode
  if (vibration > 0) return;

  // Calculate changes in acceleration
  deltx = xaxis - oldx;
  delty = yaxis - oldy;
  deltz = zaxis - oldz;

  // Update previous readings
  oldx = xaxis;
  oldy = yaxis;
  oldz = zaxis;

  // Calculate the magnitude of the impact
  magnitude = sqrt(sq(deltx) + sq(delty) + sq(deltz));

  // Check if the impact magnitude is above the sensitivity threshold
  if (magnitude >= sensitivity) {
    updateflag = 1;

    // Calculate angles using the changes in acceleration
    double X = acos((double)deltx / magnitude);
    double Y = acos((double)delty / magnitude);

    // Calculate the impact angle
    angle = (atan2(Y, X) * 180) / PI;
    angle += 180;

    // Reset anti-vibration counter
    vibration = devibrate;
  } else {
    magnitude = 0; // Reset magnitude of impact to 0
  }
}

void makeCall() {
  // Initiate a call to the specified phone number
  SerialSIM.println("ATD+27633274367;"); // Replace with the correct phone number
  updateSerial();
  delay(10000); // Call duration of 10 seconds
  SerialSIM.println("ATH"); // Hang up the call
  updateSerial();
}

void sendSMS() {
  // Send an SMS message after the call
  SerialSIM.println("AT+CMGF=1"); // Set SMS text mode
  updateSerial();
  SerialSIM.println("AT+CMGS=\"+27633274367\""); // Replace with the correct phone number
  updateSerial();
  SerialSIM.print("Crash detected! Samkele needs help");
  SerialSIM.write(26); // Send the message (ASCII code 26 is Ctrl+Z, used to send the SMS)
  updateSerial();
}

void updateSerial() {
  delay(500);

  // Forward data from Serial Monitor to SIM800L
  while (SerialMon.available()) {
    SerialSIM.write(SerialMon.read());
  }

  // Forward data from SIM800L to Serial Monitor
  while (SerialSIM.available()) {
    SerialMon.write(SerialSIM.read());
  }
}