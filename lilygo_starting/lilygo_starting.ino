#include "painlessMesh.h"
#include "SSD1306Wire.h"

#include <ArduinoJson.h>

#include <RadioLib.h>
#include "boards.h"  // Contains pin definitions (e.g., I2C_SDA=18, I2C_SCL=17)

#include "mbedtls/aes.h"

// ------------------ Mesh Definitions ------------------
#define MESH_PREFIX     "myMesh"
#define MESH_PASSWORD   "password"
#define MESH_PORT       5555
#define NODE_LABEL      "MainController"

Scheduler userScheduler;
painlessMesh mesh;
SSD1306Wire display(0x3c, I2C_SDA, I2C_SCL);  // OLED display

// Global variable for the latest plaintext JSON message from M5Stick (for display)
String latestJSON = "";

// ------------------ LoRa Definitions ------------------
#define RADIO_SCLK_PIN   5
#define RADIO_MISO_PIN   3
#define RADIO_MOSI_PIN   6
#define RADIO_CS_PIN     7
#define RADIO_RST_PIN    8
#define RADIO_DIO1_PIN   9
#define RADIO_BUSY_PIN   36
#define RADIO_RX_PIN     21   // RF switch (RX)
#define RADIO_TX_PIN     10   // RF switch (TX)

// Create an SX1280 instance for LoRa forwarding (simple style)
SX1280 loraRadio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);

// ------------------ Encryption Settings ------------------
const unsigned char aesKey[16] = { 0x01, 0x23, 0x45, 0x67,
                                   0x89, 0xAB, 0xCD, 0xEF,
                                   0xFE, 0xDC, 0xBA, 0x98,
                                   0x76, 0x54, 0x32, 0x10 };
// For CTR mode the IV (nonce) must be 16 bytes. Here we use a fixed IV.
unsigned char iv_template[16] = { 0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x01 };

// ------------------ Display Update Function ------------------
void updateDisplayBuffer(String statusMsg) {
  display.clear();
  String header = "Starting LilyGO";
  display.drawString(0, 0, header);
  display.drawString(0, 15, latestJSON);
  String statusLine = "LoRa: " + statusMsg;
  display.drawString(0, 40, statusLine);
  display.display();
}

// ------------------ Mesh Task ------------------
Task taskAnnounceNodeId(TASK_SECOND * 1, 20, [](){
  StaticJsonDocument<100> doc;
  doc["type"] = "MAIN_ID";
  doc["id"] = mesh.getNodeId();
  String jsonMsg;
  serializeJson(doc, jsonMsg);
  mesh.sendBroadcast(jsonMsg);
  Serial.println("Broadcasting: " + jsonMsg);
});

// ------------------ Mesh Callback ------------------
void receivedCallback(uint32_t from, String &msg) {
  if (from == mesh.getNodeId()) return;
  
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, msg);
  if (error) {
    Serial.println("JSON parse error: " + String(error.c_str()));
    return;
  }
  
  if (doc.containsKey("id") && doc.containsKey("status")) {
    latestJSON = msg;  // Save plaintext JSON for display
    updateDisplayBuffer("Pending");

    // --------- Encrypt the JSON payload using AES-128 CTR ----------
    size_t msgLen = msg.length();
    // Prepare a copy of the IV for this encryption call.
    unsigned char iv[16];
    memcpy(iv, iv_template, 16);
    size_t nc_off = 0;
    unsigned char stream_block[16] = {0};
    
    // Allocate a buffer for the ciphertext (same length as plaintext)
    unsigned char cipher[msgLen];
    
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    // Set encryption key (128 bits)
    mbedtls_aes_setkey_enc(&aes, aesKey, 128);
    
    // Encrypt in CTR mode. (msg.c_str() gives the plaintext bytes)
    int ret = mbedtls_aes_crypt_ctr(&aes, msgLen, &nc_off, iv, stream_block,
                                    (const unsigned char*) msg.c_str(), cipher);
    mbedtls_aes_free(&aes);
    if(ret != 0) {
      Serial.println("AES encryption failed!");
      updateDisplayBuffer("Encrypt Fail");
      return;
    }
    
    // --------- Transmit the encrypted payload over LoRa ----------
    // Transmit raw binary payload using the explicit length.
    int loraState = loraRadio.transmit((char*)cipher, msgLen);
    
    if (loraState == RADIOLIB_ERR_NONE) {
      Serial.println("LoRa forward success (encrypted).");
      updateDisplayBuffer("Success");
    } else {
      Serial.print("LoRa forward failed: ");
      Serial.println(loraState);
      updateDisplayBuffer("Failed");
    }
  } else {
    Serial.println("Unknown JSON format received.");
  }
}


// ------------------ Connection Callbacks ------------------
void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("New Connection: nodeId = %u\n", nodeId);
}
void changedConnectionCallback() {
  Serial.println("Connections changed");
}
void nodeTimeAdjustedCallback(int32_t offset) {
  //Serial.printf("Time adjusted. New time: %u, Offset: %d\n", mesh.getNodeTime(), offset);
}

// ------------------ Setup ------------------
void setup() {
  Serial.begin(115200);
  delay(100);
  
  // Initialize display
  display.init();
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.flipScreenVertically();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  delay(3000);
  
  // Initialize mesh network
  mesh.setDebugMsgTypes(ERROR | STARTUP);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
  
  Serial.println("Starting main node...");
  Serial.print("Assigned Node ID: ");
  Serial.println(mesh.getNodeId());
  
  // Update display with initial header information
  display.clear();
  display.drawString(0, 0, "Starting LilyGO");
  display.display();
  
  userScheduler.addTask(taskAnnounceNodeId);
  taskAnnounceNodeId.enable();
  
  // ----- Initialize LoRa for forwarding -----
  Serial.println("Initializing LoRa for forwarding...");
  SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN, RADIO_CS_PIN);
  pinMode(RADIO_RST_PIN, OUTPUT);
  digitalWrite(RADIO_RST_PIN, LOW); delay(20);
  digitalWrite(RADIO_RST_PIN, HIGH); delay(20);
  
  int loraState = loraRadio.begin();
  if(loraState != RADIOLIB_ERR_NONE) {
    Serial.print("LoRa begin() failed with error: ");
    Serial.println(loraState);
    while(true);
  }
  Serial.println("LoRa SX1280 initialized successfully for forwarding!");
  
  loraRadio.setRfSwitchPins(RADIO_RX_PIN, RADIO_TX_PIN);
  
  if(loraRadio.setOutputPower(13) == RADIOLIB_ERR_INVALID_OUTPUT_POWER) {
    Serial.println("Invalid output power!");
    while(true);
  }
  if(loraRadio.setFrequency(2410.5) == RADIOLIB_ERR_INVALID_FREQUENCY) {
    Serial.println("Invalid frequency!");
    while(true);
  }
  if(loraRadio.setBandwidth(203.125) == RADIOLIB_ERR_INVALID_BANDWIDTH) {
    Serial.println("Invalid bandwidth!");
    while(true);
  }
  if(loraRadio.setSpreadingFactor(10) == RADIOLIB_ERR_INVALID_SPREADING_FACTOR) {
    Serial.println("Invalid spreading factor!");
    while(true);
  }
  if(loraRadio.setCodingRate(6) == RADIOLIB_ERR_INVALID_CODING_RATE) {
    Serial.println("Invalid coding rate!");
    while(true);
  }
  Serial.println("LoRa forwarding configuration complete.");
}

// ------------------ Loop ------------------
void loop() {
  mesh.update();
}