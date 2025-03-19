#include "painlessMesh.h"
#include <M5StickCPlus.h>
#include <ArduinoJson.h>

#define MESH_PREFIX     "myMesh"
#define MESH_PASSWORD   "password"
#define MESH_PORT       5555

Scheduler userScheduler;
painlessMesh mesh;

uint32_t mainNode = 0;  // Holds the main node's ID once identified
bool mainNodeSet = false;

bool fallDetected = false; // Initial fall status: false = NO FALL DETECTED

// Callback for processing received mesh messages
void receivedCallback(uint32_t from, String &msg) {
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, msg);
  if (error) {
    Serial.println("JSON parse error: " + String(error.c_str()));
    return;
  }
  const char* type = doc["type"];
  // If we receive a main node announcement, set the main node
  if (!mainNodeSet && strcmp(type, "MAIN_ID") == 0) {
    mainNode = doc["id"];
    mainNodeSet = true;
    Serial.printf("Main node identified: %u\n", mainNode);
    updateDisplay();
  }
}

// Function to update the M5Stick display
void updateDisplay() {
  // Change background color based on fall status: red for fall detected, green for no fall
  M5.Lcd.fillScreen(fallDetected ? TFT_RED : TFT_GREEN);
  M5.Lcd.setTextColor(TFT_BLACK, fallDetected ? TFT_RED : TFT_GREEN);
  
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.printf("M5 Node\n\n");

  // Extract the last two digits of the node ID
  uint32_t nodeIdLastTwoDigits = mesh.getNodeId() % 100;
  
  // Display Mesh IP
  IPAddress ip = mesh.getAPIP();
  M5.Lcd.setTextSize(1);
  M5.Lcd.printf("Mesh IP: %s\n\n", ip.toString().c_str());
  
  // Display Node ID
  M5.Lcd.printf("Node ID: %u\n\n", nodeIdLastTwoDigits);
  
  // Display connection status
  if (mainNodeSet)
    M5.Lcd.printf("Status: Connected\n\n");
  else
    M5.Lcd.printf("Status: Not connected\n\n");
  
  // Display fall status at the bottom of the screen
  M5.Lcd.setCursor(0, 80);
  M5.Lcd.setTextSize(1.5);
  if (fallDetected)
    M5.Lcd.printf("FALL DETECTED");
  else
    M5.Lcd.printf("NO FALL");
}

void setup() {
  Serial.begin(115200);
  M5.begin();
  // Optional: set the initial time if using TimeLib (e.g., via NTP or RTC)
  // setTime(12, 34, 56, 4, 3, 2025); // Hour, minute, second, day, month, year
  
  // Initialize display background initially (green for no fall)
  updateDisplay();
  
  // Initialize mesh
  mesh.setDebugMsgTypes(ERROR | STARTUP);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
  
  delay(3000); // Allow mesh to connect
  
  // Update display with connection details
  updateDisplay();
}

void loop() {
  M5.update(); // Update button states and other internal variables
  mesh.update();
  
  // Check if Button A was pressed to toggle fall status and send data
  if (M5.BtnA.wasPressed()) {
    fallDetected = !fallDetected;  // Toggle state
    updateDisplay();  // Update screen background and status
    
    // Build JSON message
    StaticJsonDocument<200> doc;
    uint32_t nodeIdLastTwoDigits = mesh.getNodeId() % 100;  // Last two digits of Node ID
    doc["id"] = String(nodeIdLastTwoDigits);
    doc["status"] = fallDetected ? "FALL DETECTED" : "NO FALL DETECTED";
    
    String jsonMsg;
    serializeJson(doc, jsonMsg);
    
    // Send JSON message to the main node (starting LilyGO) if identified
    if (mainNodeSet) {
      mesh.sendSingle(mainNode, jsonMsg);
      Serial.printf("Sent JSON to main node %u: %s\n", mainNode, jsonMsg.c_str());
    }
    else {
      Serial.println("Main node not identified. Cannot send data.");
    }
  }
  
  delay(100);
}