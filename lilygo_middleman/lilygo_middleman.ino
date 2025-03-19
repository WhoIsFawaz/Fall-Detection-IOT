#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include <ArduinoJson.h>
#include "SSD1306Wire.h"

// I2C pins for OLED display (adjust if needed)
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
#define RADIO_RX_PIN     21   // For RF switch (RX)
#define RADIO_TX_PIN     10   // For RF switch (TX)

// LoRa max payload size for SX1280
#define MAX_LORA_PAYLOAD 255  

// Create OLED display object (SSD1306 with I2C address 0x3C)
SSD1306Wire display(0x3c, I2C_SDA, I2C_SCL);

// Create SX1280 instance for LoRa (simple style)
SX1280 loraRadio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);

// ------------------ Utility Functions ------------------
// Update OLED display with status messages
void updateDisplay(String status) {
    display.clear();
    display.drawString(0, 0, "Middleman LilyGO");
    display.drawString(0, 40, status);
    display.display();
}

// ------------------ Setup and Configuration ------------------
void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println("Initializing Middleman LilyGO (LoRa Relay)...");
  
  // Initialize display
  display.init();
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.flipScreenVertically();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  updateDisplay("LoRa Relay: Idle");
  
  // Initialize SPI for LoRa
  SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN, RADIO_CS_PIN);
  
  // Reset the LoRa module
  pinMode(RADIO_RST_PIN, OUTPUT);
  digitalWrite(RADIO_RST_PIN, LOW);
  delay(20);
  digitalWrite(RADIO_RST_PIN, HIGH);
  delay(20);
  
  // Initialize LoRa
  int state = loraRadio.begin();
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print("LoRa begin() failed with error: ");
    Serial.println(state);
    while (true);
  }
  Serial.println("LoRa SX1280 initialized successfully!");
  
  loraRadio.setRfSwitchPins(RADIO_RX_PIN, RADIO_TX_PIN);
  
  if (loraRadio.setOutputPower(13) == RADIOLIB_ERR_INVALID_OUTPUT_POWER) {
    Serial.println("Invalid output power!");
    while (true);
  }
  if (loraRadio.setFrequency(2410.5) == RADIOLIB_ERR_INVALID_FREQUENCY) {
    Serial.println("Invalid frequency!");
    while (true);
  }
  if (loraRadio.setBandwidth(203.125) == RADIOLIB_ERR_INVALID_BANDWIDTH) {
    Serial.println("Invalid bandwidth!");
    while (true);
  }
  if (loraRadio.setSpreadingFactor(10) == RADIOLIB_ERR_INVALID_SPREADING_FACTOR) {
    Serial.println("Invalid spreading factor!");
    while (true);
  }
  if (loraRadio.setCodingRate(6) == RADIOLIB_ERR_INVALID_CODING_RATE) {
    Serial.println("Invalid coding rate!");
    while (true);
  }
  Serial.println("Middleman LoRa configuration complete.");
  
  // Start continuous receive mode (blocking receive with a timeout)
  state = loraRadio.startReceive(0);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print("Failed to start continuous RX mode: ");
    Serial.println(state);
    while (true);
  }
}

void loop() {
    uint8_t receivedData[MAX_LORA_PAYLOAD]; // Buffer to store received packet
    size_t receivedLength = 0;

    // Corrected receive call (Now only 2 parameters)
    int state = loraRadio.receive(receivedData, MAX_LORA_PAYLOAD);

    // Check received data length
    receivedLength = strlen((char*)receivedData);

    // Re-arm the receiver to ensure continuous RX mode
    loraRadio.startReceive(0);

    if (state == RADIOLIB_ERR_NONE) {
        Serial.print("Middleman received packet (");
        Serial.print(receivedLength);
        Serial.println(" bytes), forwarding...");

        if (receivedLength > 0 && receivedLength <= MAX_LORA_PAYLOAD) {
            // Transmit the exact received packet without modifications
            int txState = loraRadio.transmit(receivedData, receivedLength);
            if (txState == RADIOLIB_ERR_NONE) {
                Serial.println("Relay transmission successful.");
                updateDisplay("LoRa Relay: Success");

                // Wait for 2 seconds, then return to "Idle"
                delay(2000);
                updateDisplay("LoRa Relay: Idle");

            } else {
                Serial.print("Relay transmission failed: ");
                Serial.println(txState);
                updateDisplay("LoRa Relay: Failed");
            }
        } else {
            Serial.println("Error: Received packet size out of range.");
            updateDisplay("Packet Size Error");
        }
    }
    else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
        // No message received in this cycle
    }
    else {
        Serial.print("Receive error: ");
        Serial.println(state);
        updateDisplay("Error");
    }

    delay(10);
}
