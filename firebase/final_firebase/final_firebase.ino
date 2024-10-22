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
#define SerialSIM Serial1  // Use Serial1 for SIM800L

MPU6050 mpu;
int16_t ax, ay, az;
int16_t gx, gy, gz;
int deltx = 0, delty = 0, deltz = 0;
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

// Function to format contact number as +27...
String formatContactNumber(String rawNumber) {
    if (rawNumber.startsWith("0")) {
        return "+27" + rawNumber.substring(1);
    } else {
        return rawNumber;
    }
}

void setup() {
    // Serial communication setup
    SerialMon.begin(115200);
    SerialSIM.begin(9600);   // Begin communication with SIM800L on Serial1
    Wire.begin();
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

    // Initial Firebase setup
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;
    auth.user.email = "samkelendzululeka@gmail.com";
    auth.user.password = "123456";
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
    // Impact detection based on time interval
    if (micros() - time1 > 1999) {
        Impact(); // Call Impact function to calculate and print magnitude
    }

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
    // Clear the serial monitor every 500ms
    static unsigned long lastClearTime = 0;
    if (millis() - lastClearTime >= 500) {
        lastClearTime = millis(); // Update last clear time
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
                Serial.println("Single press detected! Displaying stored contacts (offline):");
                digitalWrite(4, HIGH);
                delay(100);
                digitalWrite(4, LOW);
                delay(100);
                digitalWrite(4, HIGH);
                delay(100);
                digitalWrite(4, LOW);
                // Display stored contacts from memory
                for (int i = 0; i < contactCount; i++) {
                    Serial.print("Contact Name: ");
                    Serial.println(contactNames[i-1]);
                    Serial.print("Contact Number: ");
                    Serial.println(contactNumbers[i]);
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
        Serial.println("Attempting to read from path: " + contactPath);

        if (Firebase.RTDB.get(&fbdo, contactPath)) {
            if (fbdo.dataType() == "json") {
                FirebaseJson &json = fbdo.jsonObject();
                FirebaseJsonData jsonData;
                Serial.println("Reading contacts from Firebase:");
                contactCount = 0; // Reset contact count

                // Iterate through the JSON data
                size_t count = json.iteratorBegin();
                for (size_t i = 0; i < count && contactCount < MAX_CONTACTS; i++) {
                    String key, value;
                    int type;
                    json.iteratorGet(i, type, key, value);

                    // Check for "name" and "contact" keys
                    if (key == "name") {
                        contactNames[contactCount-1] = value;
                    } else if (key == "contact") {
                        // Convert the contact to a string if needed
                        contactNumbers[contactCount] = "+27" + value.substring(1);
                        contactCount++; // Increment contact count
                    }
                }
                json.iteratorEnd();

                // Print the names and contacts
                for (int i = 0; i < contactCount; i++) {
                    Serial.print("Contact Name: ");
                    Serial.println(contactNames[i]);
                    Serial.print("Contact Number: ");
                    Serial.println(contactNumbers[i]);
                }
                digitalWrite(4, HIGH);
                delay(1000);
                digitalWrite(4, LOW);
            }
        } else {
            Serial.println("Failed to read from Firebase");
            Serial.println("Error: " + fbdo.errorReason());
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
        updateflag = 0; // Reset update flag
    }
   // Serial.println(magnitude); 
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
// Connect to Wi-Fi
void connectWiFi() {
    Serial.print("Connecting to Wi-Fi");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }
    Serial.println("Connected to Wi-Fi");
}

// Disconnect from Wi-Fi
void disconnectWiFi() {
    Serial.println("Disconnecting from Wi-Fi...");
    WiFi.disconnect();
    Serial.println("Disconnected from Wi-Fi");
}