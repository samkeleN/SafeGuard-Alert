#include <Wire.h>
#include "I2Cdev.h"
#include "MPU6050.h"
#include <math.h>
#include <WiFiNINA.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <vector>

// MPU6050 variables
MPU6050 mpu;

// Wi-Fi and Firebase credentials
#define WIFI_SSID "sam"
#define WIFI_PASSWORD "samkelen"
#define API_KEY "AIzaSyAP-6_D-3NzPN65dWDbTWKnPPj4YzJN4Oc"
#define DATABASE_URL "https://safeguard-alert-system-default-rtdb.firebaseio.com/"

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
unsigned long readDataPrevMillis = 0;
bool signupOK = false;
String userUID = "";

// SIM800L communication uses the built-in TX and RX pins
#define SIM800_BAUD_RATE 9600

// Define maximum number of contacts
#define MAX_CONTACTS 10
String contactNames[MAX_CONTACTS];
String contactNumbers[MAX_CONTACTS];
int contactCount = 0;

// Function prototypes
void connectWiFi();
void readFirebase();
void makeCall(String number);

void setup() {
    Serial.begin(115200);      // USB serial monitor
    Serial1.begin(SIM800_BAUD_RATE);  // Use the built-in TX/RX for SIM800L communication
    Wire.begin();              // For MPU6050

    // Initialize MPU6050
    mpu.initialize();
    if (mpu.testConnection()) {
        Serial.println("MPU6050 connection successful");
    } else {
        Serial.println("MPU6050 connection failed");
    }

    // Firebase setup
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;
    auth.user.email = "samkelendzululeka@gmail.com";
    auth.user.password = "123456";

    pinMode(3, INPUT_PULLUP);  // Button pin
}

void loop() {
    // Button handling: single press to call first contact
    if (digitalRead(3) == LOW) {
        delay(500);  // Debouncing delay

        // Connect to Wi-Fi and fetch contacts from Firebase
        connectWiFi();
        Firebase.begin(&config, &auth);
        config.token_status_callback = tokenStatusCallback;

        // Check if Firebase token is valid
        if (Firebase.ready() && auth.token.uid.length() > 0) {
            signupOK = true;
            userUID = String(auth.token.uid.c_str());
            Serial.println("Authenticated as user with UID: " + userUID);
            readFirebase();  // Fetch contacts from Firebase
        }

        // Make a call to the first contact in the list
        if (contactCount > 0) {
            Serial.println("Calling first contact: " + contactNumbers[0]);
            makeCall(contactNumbers[0]);
            delay(2000);
        }

        if (contactCount > 1) {
            Serial.println("Calling second contact: " + contactNumbers[1]);
            makeCall(contactNumbers[1]);
        }

        delay(1000);
    }
}

// Function to read contacts from Firebase
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
                contactCount = 0;  // Reset contact count

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
                        contactNumbers[contactCount] = "+27" + value.substring(1);  // Format to international number
                        contactCount++;
                    }
                }
                json.iteratorEnd();

                // Print the contacts
                for (int i = 0; i < contactCount; i++) {
                    Serial.print("Contact Name: ");
                    Serial.println(contactNames[i]);
                    Serial.print("Contact Number: ");
                    Serial.println(contactNumbers[i]);
                }
            }
        } else {
            Serial.println("Failed to read from Firebase");
            Serial.println("Error: " + fbdo.errorReason());
        }
    }
}

// Function to make a call using SIM800L
void makeCall(String number) {
    Serial1.println("ATD" + number + ";");  // Dial the number
    delay(10000);  // Let the call last for 10 seconds
    Serial1.println("ATH");  // Hang up the call
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
