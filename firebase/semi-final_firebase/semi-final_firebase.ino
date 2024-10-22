#include <Wire.h>
#include "I2Cdev.h"
#include "MPU6050.h"
#include <math.h>
#include <WiFiNINA.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <vector>

#define SerialMon Serial   // Use default USB serial for monitoring

// SIM800L communication uses the built-in TX and RX pins
#define SIM800_BAUD_RATE 9600

MPU6050 mpu;
int16_t ax, ay, az;
int16_t gx, gy, gz;
int deltx = 0, delty = 0, deltz = 0, count = 0;
int deltgx = 0, deltgy = 0, deltgz = 0;
int vibration = 0, magnitude = 0;
int sensitivity = 45, devibrate = 75;
double angle;
unsigned long time1;
byte updateflag;

// Wi-Fi credentials
#define WIFI_SSID "sam"
#define WIFI_PASSWORD "samkelen"

// Firebase credentials
#define API_KEY "AIzaSyAP-6_D-3NzPN65dWDbTWKnPPj4YzJN4Oc"
#define DATABASE_URL "https://safeguard-alert-system-default-rtdb.firebaseio.com/"

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
unsigned long readDataPrevMillis = 0;
bool signupOK = false;

// Define maximum number of contacts
#define MAX_CONTACTS 10
// Variables to handle double press detection
unsigned long lastButtonPressTime = 0;
const unsigned long doublePressThreshold = 500; // 500ms for double press
bool firstPressDetected = false;

// Variables for contact data
String contactNames[MAX_CONTACTS];
String contactNumbers[MAX_CONTACTS];
int contactCount = 0;
String userUID = "";

// Function prototypes
void Impact();
void readFirebase();
void connectWiFi();
void disconnectWiFi();  // Function to disconnect from Wi-Fi
void makeCall(String number);
void sendSMS(String number);
void led_singlePress();
void led_doublePress();
void alarm();

void setup() {
    // Serial communication setup
    SerialMon.begin(9600);
    Serial1.begin(SIM800_BAUD_RATE);
    Wire.begin();
    magnitude = 0;
    mpu.initialize();

    // Check MPU6050 connection
    if (mpu.testConnection()) {
        SerialMon.println("MPU6050 connection successful");
    } else {
        SerialMon.println("MPU6050 connection failed");
    }

    time1 = micros();
    pinMode(3, INPUT_PULLUP);  // Button pin
    pinMode(4, OUTPUT); // Buzzer
    pinMode(5, INPUT_PULLUP); // False alarm

    // Initial Firebase setup
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;
    auth.user.email = "samkelendzululeka@gmail.com";
    auth.user.password = "123456";
}

void loop() {
    // Impact detection based on time interval
    if (micros() - time1 > 1999) {
        Impact(); // Call Impact function to calculate and print magnitude
    }

    // Button handling for both single and double press
    if (digitalRead(3) == LOW) {  // Button pressed
        unsigned long currentTime = millis();

        // Wait until the button is released
        while (digitalRead(3) == LOW) {
            delay(10);  // Debouncing delay
        }

        // Handle double press for connecting to Wi-Fi
        if (firstPressDetected && (currentTime - lastButtonPressTime <= doublePressThreshold)) {
            // Double press detected: Connect to Wi-Fi
            Serial.println("Double press detected! Connecting to Wi-Fi...");
            connectWiFi();

            // Reinitialize Firebase connection
            Firebase.begin(&config, &auth);
            config.token_status_callback = tokenStatusCallback;
            Firebase.reconnectWiFi(true);

            // Check token validity before reading
            if (Firebase.ready() && auth.token.uid.length() > 0) {
                signupOK = true;
                userUID = String(auth.token.uid.c_str());
                SerialMon.println("Authenticated as user with UID: " + userUID);

                // Attempt to read from Firebase
                readFirebase();
            } else {
                SerialMon.println("Failed to authenticate. Attempting to reauthenticate...");

                // Attempt to reauthenticate
                Firebase.begin(&config, &auth);
                if (Firebase.ready() && auth.token.uid.length() > 0) {
                    signupOK = true;
                    userUID = String(auth.token.uid.c_str());
                    SerialMon.println("Reauthenticated as user with UID: " + userUID);

                    // Attempt to read from Firebase
                    readFirebase();
                } else {
                    SerialMon.println("Reauthentication failed.");
                }
            }

            disconnectWiFi(); // Disconnect from Wi-Fi
            firstPressDetected = false;  // Reset double press detection
        } else {
            // Single press: Display stored contacts when offline
            firstPressDetected = true;
            lastButtonPressTime = currentTime;

            // Check if this is a single press and Wi-Fi is not connected
            if (WiFi.status() != WL_CONNECTED) {
                // Single press detected, display stored contacts
                SerialMon.println("Single press detected! Displaying stored contacts (offline):");
                led_singlePress();
                // Display stored contacts from memory
                for (int i = 0; i < contactCount; i++) {
                    SerialMon.print("Contact Name: ");
                    SerialMon.println(contactNames[i]);
                    SerialMon.print("Contact Number: ");
                    SerialMon.println(contactNumbers[i]);
                }
            }

            // Reset the first press flag after the threshold if no second press happens
            if (millis() - lastButtonPressTime > doublePressThreshold) {
                firstPressDetected = false;
            }
        }
    }
}

// Function to read from Firebase
void readFirebase() {
    if (Firebase.ready() && signupOK && (millis() - readDataPrevMillis > 5000 || readDataPrevMillis == 0)) {
        readDataPrevMillis = millis();
        String contactPath = userUID + "/NextOfKin";
        SerialMon.println("Attempting to read from path: " + contactPath);

        if (Firebase.RTDB.get(&fbdo, contactPath)) {
            if (fbdo.dataType() == "json") {
                FirebaseJson &json = fbdo.jsonObject();
                FirebaseJsonData jsonData;
                SerialMon.println("Reading contacts from Firebase:");
                contactCount = 0; // Reset contact count

                // Iterate through the JSON data
                size_t count = json.iteratorBegin();
                for (size_t i = 0; i < count && contactCount < MAX_CONTACTS; i++) {
                    String key, value;
                    int type;
                    json.iteratorGet(i, type, key, value);

                    // Check for "name" and "contact" keys
                    if (key == "name") {
                        contactNames[contactCount - 1] = value;
                    } else if (key == "contact") {
                        // Convert the contact to a string if needed
                        contactNumbers[contactCount] = "+27" + value.substring(0);
                        contactCount++; // Increment contact count
                    }
                }
                json.iteratorEnd();

                // Print the names and contacts
                for (int i = 0; i < contactCount; i++) {
                    SerialMon.print("Contact Name: ");
                    SerialMon.println(contactNames[i]);
                    SerialMon.print("Contact Number: ");
                    SerialMon.println(contactNumbers[i]);
                }
               led_doublePress();
            }
        } else {
            SerialMon.println("Failed to read from Firebase");
            SerialMon.println("Error: " + fbdo.errorReason());
        }
    }
    delay(20000); // Add a delay to prevent spamming the Firebase requests
}

// Impact detection function
void Impact() {
    time1 = micros();
    static int oldx = 0, oldy = 0, oldz = 0;
    static int oldgx = 0, oldgy = 0, oldgz = 0; // Previous gyroscope values

    // Get motion data
    mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

    int xaxis = ax / 16384.0 * 1000;
    int yaxis = ay / 16384.0 * 1000;
    int zaxis = az / 16384.0 * 1000;
    int gxaxis = gx / 131.0; // Convert gyroscope data to degrees/sec
    int gyaxis = gy / 131.0;
    int gzaxis = gz / 131.0;

    vibration--;
    if (vibration < 0) vibration = 0;
    if (vibration > 0) return;

    // Acceleration deltas
    deltx = xaxis - oldx;
    delty = yaxis - oldy;
    deltz = zaxis - oldz;
    oldx = xaxis;
    oldy = yaxis;
    oldz = zaxis;

    // Gyroscope deltas
    deltgx = gxaxis - oldgx;
    deltgy = gyaxis - oldgy;
    deltgz = gzaxis - oldgz;
    oldgx = gxaxis;
    oldgy = gyaxis;
    oldgz = gzaxis;

    magnitude = sqrt(sq(deltx) + sq(delty) + sq(deltz));

    if (magnitude >= sensitivity) {
        updateflag = 1;
        double X = acos((double)deltx / magnitude);
        double Y = acos((double)delty / magnitude);
        double Z = acos((double)deltz / magnitude);
        angle = (X + Y + Z) * 57.29577951; // Convert to degrees
    }

    if (updateflag == 1) {
        vibration = devibrate;
        SerialMon.print("Magnitude: ");
        SerialMon.println(magnitude);
        SerialMon.print("Angle: ");
        SerialMon.println(angle);
        count = 0;
        if(magnitude > 1000){
          alarm();
          if(count > 9){
            // If button is not pressed, continue with crash detection
            SerialMon.println("crash detected");
            SerialMon.println("Calling your next of kin for help");

            // Make a call to the first contact in the list
            if (contactCount > 0) {
              Serial.println("Calling first contact: " + contactNumbers[0]);
              makeCall(contactNumbers[0]);
              delay(2000);
              Serial.println("Calling second contact: " + contactNumbers[1]);
              makeCall(contactNumbers[1]);
              delay(2000);

              Serial.println("Sending SMS to first contact: " + contactNumbers[0]);
              sendSMS(contactNumbers[0]);
              delay(2000);
              Serial.println("Sending SMS to second contact: " + contactNumbers[1]);
              sendSMS(contactNumbers[1]);
              delay(1000);
            }

            if (contactCount > 1) {
            //   Serial.println("");
            //    delay(1000);
            //    Serial.println("Calling second contact: " + contactNumbers[1]);
            //    makeCall(contactNumbers[1]);
            //    delay(2000);
              //  Serial.println("Sending SMS to second contact: " + contactNumbers[1]);
              //  sendSMS(contactNumbers[1]);
              //  delay(20);
            } 
          }        
            SerialMon.println("Welcome to Safeguard Alert System");
        }
    updateflag = 0; // Reset update flag
  }
}

// Connect to Wi-Fi
void connectWiFi() {
    SerialMon.print("Connecting to Wi-Fi");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        SerialMon.print(".");
        delay(500);
    }
    SerialMon.println("");
    SerialMon.println("Connected to Wi-Fi");
}

// Disconnect from Wi-Fi
void disconnectWiFi() {
    Serial.println("Disconnecting from Wi-Fi...");
    WiFi.disconnect();
    Serial.println("Disconnected from Wi-Fi");
}

// Function to make a call using SIM800L
void makeCall(String number) {
    Serial1.println("ATD" + number + ";");  // Dial the number
    delay(15000);  // Let the call last for 10 seconds
    Serial1.println("ATH");  // Hang up the call
}

void sendSMS(String number) {
  // Send an SMS message after the call
  Serial1.println("AT+CMGF=1"); // Set SMS text mode
  updateSerial();
  Serial1.println("AT+CMGS=\""+ number +"\""); // Replace with the correct phone number
  updateSerial();
  Serial1.print("Crash detected! Samkele needs help. Here is his location: https://maps.app.goo.gl/N2D6o9qhjmi7iYKn8");
  Serial1.write(26); // Send the message (ASCII code 26 is Ctrl+Z, used to send the SMS)
  updateSerial();
}

void led_singlePress(){
    digitalWrite(4, HIGH);
    delay(100);
    digitalWrite(4, LOW);
    delay(100);
    digitalWrite(4, HIGH);
    delay(100);
    digitalWrite(4, LOW);
    delay(100);
}

void led_doublePress(){
  digitalWrite(4, HIGH);
  delay(1000);
  digitalWrite(4, LOW);
}

void alarm(){
  for(int i = 0; i < 11; i++){
    digitalWrite(4, HIGH);
    delay(500);
    digitalWrite(4, LOW);
    delay(500);
    // Check if false alarm button is pressed
    if (digitalRead(5) == LOW) {  // Assuming LOW means the button is pressed
      SerialMon.println("False alarm, exiting crash detection.");
      return;  // Exit the crash detection logic
    }
    count++;
    SerialMon.println(count);
  }
}

void updateSerial() {
  delay(500);

  // Forward data from Serial Monitor to SIM800L
  while (SerialMon.available()) {
    Serial1.write(SerialMon.read());
  }

  // Forward data from SIM800L to Serial Monitor
  while (Serial1.available()) {
    SerialMon.write(Serial1.read());
  }
}