#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include <ArduinoJson.h>
#include "SSD1306Wire.h"
#include "mbedtls/aes.h"  // Include mbedTLS AES header
#include <WiFi.h>
#include <HTTPClient.h>

// Wi-Fi credentials
const char* WIFI_SSID = "M1";
const char* WIFI_PASS = "123456789";

// Flask server details
const char* SERVER_URL = "http://172.20.10.3:5000/update_status";  // Update with Flask server IP


// I2C pins for OLED display
#define I2C_SDA 18
#define I2C_SCL 17

// LoRa pin definitions (from your board.h)
#define RADIO_SCLK_PIN   5
#define RADIO_MISO_PIN   3
#define RADIO_MOSI_PIN   6
#define RADIO_CS_PIN     7
#define RADIO_RST_PIN    8
#define RADIO_DIO1_PIN   9
#define RADIO_BUSY_PIN   36
#define RADIO_RX_PIN     21  // RF switch for RX
#define RADIO_TX_PIN     10  // RF switch for TX

// Create OLED display object (SSD1306 with I2C address 0x3C)
SSD1306Wire display(0x3c, I2C_SDA, I2C_SCL);

// Create SX1280 instance for LoRa receiving
SX1280 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);

// Global variable for the latest JSON message
String latestJSON = "";

// ------------------ Encryption Settings ------------------
// Pre-shared AES-128 key (16 bytes)
const unsigned char aesKey[16] = { 0x01, 0x23, 0x45, 0x67,
                                   0x89, 0xAB, 0xCD, 0xEF,
                                   0xFE, 0xDC, 0xBA, 0x98,
                                   0x76, 0x54, 0x32, 0x10 };
// Fixed IV (nonce) for CTR mode (16 bytes)
unsigned char iv_template[16] = { 0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x01 };

// ------------------ Utility Functions ------------------

// Clean received JSON message by ensuring it ends at '}'
String cleanJson(const String &msg) {
  int firstBrace = msg.indexOf('}');
  if (firstBrace != -1) {
    return msg.substring(0, firstBrace + 1);
  }
  return msg;
}

// Connect to WiFi
void connectWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(1000);
    }
    Serial.println(" Connected!");
}

// Send decrypted data to Flask server
void sendToServer(String id, String status) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[ERROR] WiFi Disconnected. Retrying...");
        connectWiFi();  // Try reconnecting
        delay(2000);
        return;
    }

    HTTPClient http;
    Serial.print("----------------------------------------------\n");
    Serial.print("Connecting to server: ");
    Serial.println(SERVER_URL);

    http.begin(SERVER_URL);
    http.addHeader("Content-Type", "application/json");
    http.useHTTP10(true);  // Force HTTP 1.0 (fixes some ESP32 issues)
    http.setTimeout(5000); // Increase timeout to 5 seconds

    // Create JSON payload
    StaticJsonDocument<200> jsonDoc;
    jsonDoc["id"] = id;
    jsonDoc["status"] = status;
    String jsonPayload;
    serializeJson(jsonDoc, jsonPayload);

    Serial.print("[INFO] Sending Data to server: ");
    Serial.println(jsonPayload);
    
    int httpResponseCode = http.POST(jsonPayload);

    if (httpResponseCode > 0) {
        Serial.print("[SUCCESS] Server Response Code: ");
        Serial.println(httpResponseCode);
        Serial.print("[SUCCESS] Server Response JSON: ");
        Serial.println(http.getString());
    } else {
        Serial.print("[ERROR] HTTP Error: ");
        Serial.println(httpResponseCode);
    }
    Serial.print("----------------------------------------------\n");
    http.end();
}
// Update OLED display with header and latest JSON message
void updateDisplay() {
  display.clear();
  display.drawString(0, 0, "Final LilyGO");
  display.drawString(0, 15, latestJSON);
  display.display();
}

// ------------------ Setup ------------------

void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println("Initializing Final Destination (LoRa RX)...");

  // Connect to WiFi
  connectWiFi();
  
  // Initialize SPI for LoRa
  SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN, RADIO_CS_PIN);
  
  // Reset LoRa module
  pinMode(RADIO_RST_PIN, OUTPUT);
  digitalWrite(RADIO_RST_PIN, LOW);
  delay(20);
  digitalWrite(RADIO_RST_PIN, HIGH);
  delay(20);
  
  int state = radio.begin();
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print("LoRa begin() failed with error: ");
    Serial.println(state);
    while (true);
  }
  Serial.println("LoRa SX1280 initialized successfully!");
  
  radio.setRfSwitchPins(RADIO_RX_PIN, RADIO_TX_PIN);
  
  if (radio.setOutputPower(13) == RADIOLIB_ERR_INVALID_OUTPUT_POWER) {
    Serial.println("Invalid output power!");
    while (true);
  }
  if (radio.setFrequency(2410.5) == RADIOLIB_ERR_INVALID_FREQUENCY) {
    Serial.println("Invalid frequency!");
    while (true);
  }
  if (radio.setBandwidth(203.125) == RADIOLIB_ERR_INVALID_BANDWIDTH) {
    Serial.println("Invalid bandwidth!");
    while (true);
  }
  if (radio.setSpreadingFactor(10) == RADIOLIB_ERR_INVALID_SPREADING_FACTOR) {
    Serial.println("Invalid spreading factor!");
    while (true);
  }
  if (radio.setCodingRate(6) == RADIOLIB_ERR_INVALID_CODING_RATE) {
    Serial.println("Invalid coding rate!");
    while (true);
  }
  Serial.println("Final Destination LoRa configuration complete.");
  
  // Initialize the display
  display.init();
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.flipScreenVertically();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  updateDisplay();
  
  // Start continuous receive mode
  state = radio.startReceive(0);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print("Failed to start continuous RX mode: ");
    Serial.println(state);
    while (true);
  }
}

// ------------------ Main Loop ------------------

void loop() {
    static String receivedMessages[2];  // Buffer to store up to 2 received messages
    static uint8_t messageCount = 0;
    static unsigned long lastReceiveTime = 0;

    String receivedMsg;
    int state = radio.receive(receivedMsg, 500);  // Wait for 500ms for a packet

    if (state == RADIOLIB_ERR_NONE) {
        int msgLen = receivedMsg.length();
        if (msgLen <= 0) return;

        // Copy received data into a buffer for decryption
        uint8_t cipher[msgLen];
        receivedMsg.getBytes(cipher, msgLen + 1);

        uint8_t decrypted[msgLen];
        size_t nc_off = 0;
        unsigned char stream_block[16] = {0};
        unsigned char iv[16];
        memcpy(iv, iv_template, 16);

        mbedtls_aes_context aes;
        mbedtls_aes_init(&aes);
        mbedtls_aes_setkey_enc(&aes, aesKey, 128);
        int ret = mbedtls_aes_crypt_ctr(&aes, msgLen, &nc_off, iv, stream_block, cipher, decrypted);
        mbedtls_aes_free(&aes);
        if (ret != 0) {
            Serial.println("[ERROR] AES decryption failed!");
            return;
        }

        // Convert decrypted output to a string
        char plainText[msgLen + 1];
        memcpy(plainText, decrypted, msgLen);
        plainText[msgLen] = '\0';

        String plainString = String(plainText);
        plainString = cleanJson(plainString);
        plainString.trim();

        if (plainString.length() > 0) {
            Serial.print("[INFO] Decrypted: ");
            Serial.println(plainString);

            // Store the received message in the buffer
            if (messageCount < 2) {
                receivedMessages[messageCount] = plainString;
                messageCount++;
            }

            // Update last receive timestamp
            lastReceiveTime = millis();
        }
    } 
    else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
        // No message received within timeout, continue waiting
    } 
    else {
        Serial.print("[ERROR] Receive error: ");
        Serial.println(state);
    }

    // Wait for additional messages for 200ms before sending to Flask
    if (messageCount > 0 && millis() - lastReceiveTime > 200) {
        if (messageCount == 2 && receivedMessages[0] == receivedMessages[1]) {
            Serial.println("[INFO] Both messages match, sending to server...");
            latestJSON = receivedMessages[0];
        } else {
            Serial.println("[INFO] Single or mismatched messages, using first one...");
            latestJSON = receivedMessages[0];
        }

        updateDisplay();

        // Parse JSON data
        StaticJsonDocument<200> jsonDoc;
        DeserializationError error = deserializeJson(jsonDoc, latestJSON);
        if (error) {
            Serial.println("[ERROR] JSON parsing failed!");
            return;
        }

        String id = jsonDoc["id"];
        String status = jsonDoc["status"];

        // Send to Flask server
        sendToServer(id, status);

        // Reset message buffer
        messageCount = 0;
    }

    delay(10);
}