#include <Wire.h>
#include "I2Cdev.h"
#include "MPU6050.h"
#include <math.h>

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
  Serial.begin(9600);
  Wire.begin();
  mpu.initialize();

  // Check MPU6050 connection
  if (mpu.testConnection()) {
    Serial.println("MPU6050 connection successful");
  } else {
    Serial.println("MPU6050 connection failed");
  }

  time1 = micros();
}

void loop() {
  // Call impact routine every 2ms
  if (micros() - time1 > 1999) Impact();
  Serial.println(magnitude);
  if (magnitude > 3000) {
  // Display impact information if detected
    if (updateflag > 0) {
      updateflag = 0;
      Serial.print("Impact detected!! ");
      Serial.print("Magnitude: ");
      Serial.print(magnitude);
      Serial.print("\t Angle: ");
      Serial.println(angle, 2);
      delay(2000);
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
