/**
 * DMX LoRa Control System for Heltec LoRa 32 V3
 * 
 * Receives JSON commands over The Things Network (TTN) and translates them
 * into DMX signals to control multiple DMX lighting fixtures.
 * 
 * Created for US915 frequency plan (United States)
 * Uses OTAA activation for TTN
 * 
 * Supported JSON Commands:
 * 
 * 1. Direct DMX Control:
 * {
 *   "lights": [
 *     {
 *       "address": 1,
 *       "channels": [255, 0, 128, 0]
 *     },
 *     {
 *       "address": 5,
 *       "channels": [255, 255, 100, 0]
 *     }
 *   ]
 * }
 * 
 * 2. Rainbow Chase Test Pattern:
 * {
 *   "test": {
 *     "pattern": "rainbow",
 *     "cycles": 3,        // Optional: number of cycles (1-10)
 *     "speed": 50,        // Optional: ms between updates (10-500)
 *     "staggered": true   // Optional: create chase effect across fixtures
 *   }
 * }
 * 
 * 3. Strobe Test Pattern:
 * {
 *   "test": {
 *     "pattern": "strobe", 
 *     "color": 1,         // Optional: 0=white, 1=red, 2=green, 3=blue
 *     "count": 20,        // Optional: number of flashes (1-100)
 *     "onTime": 50,       // Optional: ms for on phase (10-1000)
 *     "offTime": 50,      // Optional: ms for off phase (10-1000)
 *     "alternate": false  // Optional: alternate between fixtures
 *   }
 * }
 * 
 * 4. Continuous Rainbow Mode:
 * {
 *   "test": {
 *     "pattern": "continuous",
 *     "enabled": true,    // Optional: true to enable, false to disable
 *     "speed": 30,        // Optional: ms between updates (5-500)
 *     "staggered": true   // Optional: create chase effect across fixtures
 *   }
 * }
 * 
 * 5. Ping Test (for testing downlink communication):
 * {
 *   "test": {
 *     "pattern": "ping"
 *   }
 * }
 * 
 * Libraries:
 * - LoRaManager: Custom LoRaWAN communication via RadioLib
 * - ArduinoJson: JSON parsing
 * - DmxController: DMX output control
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPI.h>  // Include SPI library explicitly
#include "LoRaManager.h"
#include "DmxController.h"
#include "Logger.h"

// Debug output
#define SERIAL_BAUD 115200

// Diagnostic LED pin
#define LED_PIN 35  // Built-in LED on Heltec LoRa 32 V3

// DMX configuration for Heltec LoRa 32 V3
#define DMX_PORT 1
#define DMX_TX_PIN 19  // TX pin for DMX
#define DMX_RX_PIN 20  // RX pin for DMX
#define DMX_DIR_PIN 5  // DIR pin for DMX (connect to both DE and RE on MAX485)

// LoRaWAN TTN Connection Parameters
#define LORA_CS_PIN   8     // Corrected CS pin for Heltec LoRa 32 V3
#define LORA_DIO1_PIN 14    // DIO1 pin
#define LORA_RESET_PIN 12   // Reset pin  
#define LORA_BUSY_PIN 13    // Busy pin

// SPI pins for Heltec LoRa 32 V3
#define LORA_SPI_SCK  9   // SPI clock
#define LORA_SPI_MISO 11  // SPI MISO
#define LORA_SPI_MOSI 10  // SPI MOSI

// LoRaWAN Credentials (can be changed by the user)
uint64_t joinEUI = 0x0000000000000001; // 0000000000000001
uint64_t devEUI = 0x70B3D57ED80041B2;  // 70B3D57ED80041B2
uint8_t appKey[] = {0x45, 0xD3, 0x7B, 0xF3, 0x77, 0x61, 0xA6, 0x1F, 0x9F, 0x07, 0x1F, 0xE1, 0x6D, 0x4F, 0x57, 0x77}; // 45D37BF37761A61F9F071FE16D4F5777
uint8_t nwkKey[] = {0x45, 0xD3, 0x7B, 0xF3, 0x77, 0x61, 0xA6, 0x1F, 0x9F, 0x07, 0x1F, 0xE1, 0x6D, 0x4F, 0x57, 0x77}; // Same as appKey for OTAA

// DMX configuration - we'll use dynamic configuration from JSON
#define MAX_FIXTURES 32           // Maximum number of fixtures supported
#define MAX_CHANNELS_PER_FIXTURE 16 // Maximum channels per fixture
#define MAX_JSON_SIZE 1024        // Maximum size of JSON document

// Global variables
bool dmxInitialized = false;
bool loraInitialized = false;
DmxController* dmx = NULL;
LoRaManager* lora = NULL;

// Forward declaration of the callback function
void handleDownlinkCallback(uint8_t* payload, size_t size, uint8_t port);

// Placeholder for the received data
uint8_t receivedData[MAX_JSON_SIZE];
size_t receivedDataSize = 0;
uint8_t receivedPort = 0;
bool dataReceived = false;

// Global variables for continuous rainbow effect
bool runningRainbowDemo = false;  // Controls continuous rainbow effect
unsigned long lastRainbowStep = 0; // Timestamp for last rainbow step
uint32_t rainbowStepCounter = 0;  // Step counter for rainbow effect
int rainbowStepDelay = 30;        // Delay between steps in milliseconds
bool rainbowStaggered = true;     // Whether to stagger colors across fixtures

/**
 * Process JSON payload and control DMX fixtures
 * 
 * Expected JSON format:
 * {
 *   "lights": [
 *     {
 *       "address": 1,
 *       "channels": [255, 0, 128, 0]
 *     },
 *     {
 *       "address": 5,
 *       "channels": [255, 255, 100, 0]
 *     }
 *   ]
 * }
 * 
 * Or for test patterns:
 * {
 *   "test": {
 *     "pattern": "rainbow",
 *     "cycles": 3,
 *     "speed": 50,
 *     "staggered": true
 *   }
 * }
 * 
 * Or:
 * {
 *   "test": {
 *     "pattern": "strobe",
 *     "color": 1,
 *     "count": 20,
 *     "onTime": 50,
 *     "offTime": 50,
 *     "alternate": false
 *   }
 * }
 * 
 * @param jsonString The JSON string to process
 * @return true if processing was successful, false otherwise
 */
bool processJsonPayload(const String& jsonString) {
  Logger::jsonf("Processing JSON payload: %s", jsonString.c_str());
  
  // Create a JSON document
  StaticJsonDocument<MAX_JSON_SIZE> doc;
  
  // Parse the JSON string
  DeserializationError error = deserializeJson(doc, jsonString);
  
  // Check for parsing errors
  if (error) {
    Logger::errorf("JSON parsing failed: %s", error.c_str());
    return false;
  }
  
  // Check if this is a test pattern command
  if (doc.containsKey("test")) {
    // Get the test object
    JsonObject testObj = doc["test"];
    
    // Check if the test object has a pattern field
    if (!testObj.containsKey("pattern")) {
      Logger::error("JSON format error: 'pattern' field not found in test object");
      return false;
    }
    
    // Get the pattern type
    String pattern = testObj["pattern"].as<String>();
    pattern.toLowerCase(); // Convert to lowercase for case-insensitive comparison
    
    // Process based on pattern type
    if (pattern == "rainbow") {
      // Get parameters with defaults if not specified
      int cycles = testObj.containsKey("cycles") ? testObj["cycles"].as<int>() : 3;
      int speed = testObj.containsKey("speed") ? testObj["speed"].as<int>() : 50;
      bool staggered = testObj.containsKey("staggered") ? testObj["staggered"].as<bool>() : true;
      
      // Validate parameters
      cycles = max(1, min(cycles, 10)); // Limit cycles between 1 and 10
      speed = max(10, min(speed, 500)); // Limit speed between 10ms and 500ms
      
      Logger::testf("Executing rainbow chase pattern via downlink command");
      Logger::testf("Cycles: %d, Speed: %dms, Staggered: %s",
                    cycles, speed, staggered ? "Yes" : "No");
      
      // Configure test fixtures if none exist
      if (dmx->getNumFixtures() == 0) {
        Logger::system("Setting up default test fixtures for rainbow pattern");
        // Initialize 4 test fixtures with 4 channels each (RGBW)
        dmx->initializeFixtures(4, 4);
        
        // Configure fixtures with sequential DMX addresses
        dmx->setFixtureConfig(0, "Fixture 1", 1, 1, 2, 3, 4);
        dmx->setFixtureConfig(1, "Fixture 2", 5, 5, 6, 7, 8);
        dmx->setFixtureConfig(2, "Fixture 3", 9, 9, 10, 11, 12);
        dmx->setFixtureConfig(3, "Fixture 4", 13, 13, 14, 15, 16);
      }
      
      // Run the rainbow chase pattern
      dmx->runRainbowChase(cycles, speed, staggered);
      
      return true;
    } 
    else if (pattern == "strobe") {
      // Get parameters with defaults if not specified
      int color = testObj.containsKey("color") ? testObj["color"].as<int>() : 0;
      int count = testObj.containsKey("count") ? testObj["count"].as<int>() : 20;
      int onTime = testObj.containsKey("onTime") ? testObj["onTime"].as<int>() : 50;
      int offTime = testObj.containsKey("offTime") ? testObj["offTime"].as<int>() : 50;
      bool alternate = testObj.containsKey("alternate") ? testObj["alternate"].as<bool>() : false;
      
      // Validate parameters
      color = max(0, min(color, 3)); // Limit color between 0 and 3
      count = max(1, min(count, 100)); // Limit count between 1 and 100
      onTime = max(10, min(onTime, 1000)); // Limit onTime between 10ms and 1000ms
      offTime = max(10, min(offTime, 1000)); // Limit offTime between 10ms and 1000ms
      
      Logger::testf("Executing strobe test pattern via downlink command");
      Logger::testf("Color: %d, Count: %d, On Time: %dms, Off Time: %dms, Alternate: %s",
                   color, count, onTime, offTime, alternate ? "Yes" : "No");
      
      // Configure test fixtures if none exist
      if (dmx->getNumFixtures() == 0) {
        Logger::system("Setting up default test fixtures for strobe pattern");
        // Initialize 4 test fixtures with 4 channels each (RGBW)
        dmx->initializeFixtures(4, 4);
        
        // Configure fixtures with sequential DMX addresses
        dmx->setFixtureConfig(0, "Fixture 1", 1, 1, 2, 3, 4);
        dmx->setFixtureConfig(1, "Fixture 2", 5, 5, 6, 7, 8);
        dmx->setFixtureConfig(2, "Fixture 3", 9, 9, 10, 11, 12);
        dmx->setFixtureConfig(3, "Fixture 4", 13, 13, 14, 15, 16);
      }
      
      // Run the strobe test pattern
      dmx->runStrobeTest(color, count, onTime, offTime, alternate);
      
      return true;
    }
    else if (pattern == "continuous") {
      // This pattern controls the continuous rainbow effect in the main loop
      
      // Get parameters with defaults if not specified
      bool enabled = testObj.containsKey("enabled") ? testObj["enabled"].as<bool>() : false;
      int speed = testObj.containsKey("speed") ? testObj["speed"].as<int>() : 30;
      bool staggered = testObj.containsKey("staggered") ? testObj["staggered"].as<bool>() : true;
      
      // Validate parameters
      speed = max(5, min(speed, 500)); // Limit speed between 5ms and 500ms
      
      // Set the continuous rainbow mode
      runningRainbowDemo = enabled;
      rainbowStepDelay = speed;
      rainbowStaggered = staggered;
      
      Logger::testf("Continuous rainbow mode: %s, Speed: %dms, Staggered: %s",
                   enabled ? "ENABLED" : "DISABLED", speed, staggered ? "Yes" : "No");
      
      // Configure test fixtures if none exist
      if (dmx->getNumFixtures() == 0) {
        Logger::system("Setting up default test fixtures for continuous rainbow");
        // Initialize 4 test fixtures with 4 channels each (RGBW)
        dmx->initializeFixtures(4, 4);
        
        // Configure fixtures with sequential DMX addresses
        dmx->setFixtureConfig(0, "Fixture 1", 1, 1, 2, 3, 4);
        dmx->setFixtureConfig(1, "Fixture 2", 5, 5, 6, 7, 8);
        dmx->setFixtureConfig(2, "Fixture 3", 9, 9, 10, 11, 12);
        dmx->setFixtureConfig(3, "Fixture 4", 13, 13, 14, 15, 16);
      }
      
      // Clear all channels if disabling
      if (!enabled) {
        dmx->clearAllChannels();
        dmx->sendData();
      }
      
      return true;
    }
    else if (pattern == "ping") {
      // Simple ping command for testing downlink connectivity
      Logger::test("=== PING RECEIVED ===");
      Logger::test("Downlink communication is working!");
      
      // Blink the LED in a distinctive pattern to indicate ping received
      for (int i = 0; i < 3; i++) {
        DmxController::blinkLED(LED_PIN, 3, 100);
        delay(500);
      }
      
      // Send a ping response uplink
      if (loraInitialized && lora != NULL) {
        String response = "{\"ping_response\":\"ok\"}";
        if (lora->sendString(response, 1, true)) {
          Logger::system("Ping response sent (confirmed)");
        }
      }
      
      return true;
    }
    else {
      Logger::errorf("Unknown test pattern: %s", pattern.c_str());
      return false;
    }
  }
  // Check if this is a standard lights control command
  else if (doc.containsKey("lights")) {
    // Check if the JSON has a "lights" array
    if (!doc["lights"].is<JsonArray>()) {
      Logger::error("JSON format error: 'lights' array not found");
      return false;
    }
    
    // Get the lights array
    JsonArray lights = doc["lights"].as<JsonArray>();
    
    // Clear all DMX channels before setting new values
    dmx->clearAllChannels();
    
    // Process each light in the array
    for (JsonObject light : lights) {
      // Check if light has required fields
      if (!light.containsKey("address") || !light.containsKey("channels")) {
        Logger::error("JSON format error: light missing required fields");
        continue;  // Skip this light
      }
      
      // Get the DMX start address
      int address = light["address"].as<int>();
      
      // Get the channels array
      JsonArray channels = light["channels"].as<JsonArray>();
      
      Logger::dmxf("Setting DMX channels for fixture at address %d: ", address);
      
      // Set the channel values
      int channelIndex = 0;
      for (JsonVariant value : channels) {
        uint8_t dmxValue = value.as<uint8_t>();
        // Set the DMX value for this channel
        dmx->getDmxData()[address + channelIndex] = dmxValue;
        
        Logger::dmxf("%d ", dmxValue);
        
        channelIndex++;
      }
      Logger::dmx("");
    }
    
    // Send the updated DMX data
    dmx->sendData();
    
    Logger::dmx("=== DMX COMMAND RECEIVED ===");
    Logger::dmxf("Processing %d light fixtures", lights.size());
    
    return true;
  }
  else {
    Logger::error("JSON format error: missing 'lights' or 'test' object");
    return false;
  }
}

/**
 * Callback function for receiving downlink data from LoRaWAN
 * This function will be called by the LoRaManager when data is received
 * 
 * @param payload The payload data
 * @param size The size of the payload
 * @param port The port on which the data was received
 */
void handleDownlinkCallback(uint8_t* payload, size_t size, uint8_t port) {
  Logger::loraf("Downlink callback triggered on port %d, size: %d", port, size);
  
  // Store the data for processing in the main loop
  if (size <= MAX_JSON_SIZE) {
    memcpy(receivedData, payload, size);
    receivedDataSize = size;
    receivedPort = port;
    dataReceived = true;
    
    // Blink LED once to indicate data reception
    digitalWrite(LED_PIN, HIGH);
    delay(50);
    digitalWrite(LED_PIN, LOW);
  } else {
    Logger::error("ERROR: Received payload exceeds buffer size");
  }
}

/**
 * Handle incoming downlink data from LoRaWAN
 * 
 * @param payload The payload data
 * @param size The size of the payload
 * @param port The port on which the data was received
 */
void handleDownlink(uint8_t* payload, size_t size, uint8_t port) {
  Logger::loraf("Received downlink on port %d, size: %d", port, size);
  
  // Convert payload to string
  String payloadStr = "";
  for (size_t i = 0; i < size; i++) {
    payloadStr += (char)payload[i];
  }
  
  // Process the payload as JSON
  if (dmxInitialized) {
    bool success = processJsonPayload(payloadStr);
    if (success) {
      // Blink LED to indicate successful processing
      DmxController::blinkLED(LED_PIN, 2, 200);
    } else {
      // Blink LED rapidly to indicate error
      DmxController::blinkLED(LED_PIN, 5, 100);
    }
  } else {
    Logger::error("DMX not initialized, cannot process payload");
    // Blink LED rapidly to indicate error
    DmxController::blinkLED(LED_PIN, 5, 100);
  }
}

void setup() {
  // Initialize logger first with all categories enabled
  Logger::begin(SERIAL_BAUD);
  delay(2000);
  
  // Set up LED pin
  pinMode(LED_PIN, OUTPUT);
  
  // Initial blink to indicate we're alive
  DmxController::blinkLED(LED_PIN, 2, 500);
  
  // Print startup message
  Logger::system("===== DMX LoRa Control System =====");
  Logger::system("Initializing...");
  
  // Print ESP-IDF version
  Logger::systemf("ESP-IDF Version: %s", ESP.getSdkVersion());
  
  // Print memory info
  Logger::systemf("Free heap at startup: %d", ESP.getFreeHeap());
  
  // Print LoRa pin configuration for debugging
  Logger::system("\nLoRa Pin Configuration:");
  Logger::debugf("CS Pin: %d", LORA_CS_PIN);
  Logger::debugf("DIO1 Pin: %d", LORA_DIO1_PIN);
  Logger::debugf("Reset Pin: %d", LORA_RESET_PIN);
  Logger::debugf("Busy Pin: %d", LORA_BUSY_PIN);
  Logger::debugf("SPI SCK: %d, MISO: %d, MOSI: %d", LORA_SPI_SCK, LORA_SPI_MISO, LORA_SPI_MOSI);
  Logger::lora("Frequency: Default to US915 (United States) - but configurable to other bands");
  Logger::lora("Default frequency band is US915 with subband 2");
  
  // Initialize SPI for LoRa
  SPI.begin(LORA_SPI_SCK, LORA_SPI_MISO, LORA_SPI_MOSI);
  Logger::lora("SPI initialized for LoRa");
  delay(100); // Short delay after SPI init
  
  // Initialize LoRaWAN
  Logger::lora("Initializing LoRaWAN...");
  
  try {
    // Create delay before LoRa initialization
    delay(200);
    
    // Create a new LoRaManager with default US915 band and subband 2 (can be changed for other regions)
    lora = new LoRaManager(US915, 2);
    Logger::system("LoRaManager instance created, attempting to initialize radio...");
    
    // Add delay before begin
    delay(100);
    
    if (lora->begin(LORA_CS_PIN, LORA_DIO1_PIN, LORA_RESET_PIN, LORA_BUSY_PIN)) {
      Logger::system("LoRaWAN module initialized successfully!");
      
      // Set credentials
      lora->setCredentials(joinEUI, devEUI, appKey, nwkKey);
      Logger::system("LoRaWAN credentials set");
      
      // Register the downlink callback
      lora->setDownlinkCallback(handleDownlinkCallback);
      Logger::system("Downlink callback registered");
      
      // Join network
      Logger::system("Joining LoRaWAN network...");
      if (lora->joinNetwork()) {
        Logger::system("Successfully joined LoRaWAN network!");
        
        // Add delay to allow session establishment to complete
        Logger::system("Waiting for session establishment...");
        delay(7000);  // 7 second delay after join for more reliable session establishment
        
        loraInitialized = true;
        DmxController::blinkLED(LED_PIN, 3, 300);  // 3 blinks for successful join
      } else {
        Logger::errorf("Failed to join LoRaWAN network. Error code: %d", lora->getLastErrorCode());
        DmxController::blinkLED(LED_PIN, 4, 200);  // 4 blinks for join failure
      }
    } else {
      Logger::errorf("Failed to initialize LoRaWAN. Error code: %d", lora->getLastErrorCode());
      DmxController::blinkLED(LED_PIN, 5, 100);  // 5 blinks for init failure
    }
  } catch (...) {
    Logger::error("ERROR: Exception during LoRaWAN initialization!");
    lora = NULL;
  }
  
  // Initialize DMX with error handling
  Logger::system("Initializing DMX controller...");
  
  try {
    dmx = new DmxController(DMX_PORT, DMX_TX_PIN, DMX_RX_PIN, DMX_DIR_PIN);
    Logger::system("DMX controller object created");
    
    // Initialize DMX
    dmx->begin();
    dmxInitialized = true;
    Logger::system("DMX controller initialized successfully!");
    
    // Set initial values to zero and send to make sure fixtures are clear
    dmx->clearAllChannels();
    dmx->sendData();
    Logger::system("DMX channels cleared");
  } catch (...) {
    Logger::error("ERROR: Exception during DMX initialization!");
    dmx = NULL;
    dmxInitialized = false;
    DmxController::blinkLED(LED_PIN, 5, 100);  // Error indicator
  }
  
  Logger::system("Setup complete!");
  Logger::systemf("Free heap after setup: %d", ESP.getFreeHeap());
  
  // Send an uplink message to confirm device is online
  if (loraInitialized) {
    // Add delay before first transmission to ensure network is ready
    Logger::system("Preparing to send first uplink...");
    delay(2000);  // 2 second delay before first uplink
    
    String message = "{\"status\":\"online\",\"dmx\":" + String(dmxInitialized ? "true" : "false") + "}";
    if (lora->sendString(message, 1, true)) { // Changed to confirmed uplink
      Logger::system("Status uplink sent successfully (confirmed)");
    } else {
      Logger::error("Failed to send status uplink");
    }
  }
  
  // If DMX is initialized, set up fixtures but don't run automatic demos
  if (dmxInitialized) {
    // Configure some test fixtures if none exist
    if (dmx->getNumFixtures() == 0) {
      Logger::system("Setting up test fixtures...");
      // Initialize 4 test fixtures with 4 channels each (RGBW)
      dmx->initializeFixtures(4, 4);
      
      // Configure fixtures with sequential DMX addresses
      dmx->setFixtureConfig(0, "Fixture 1", 1, 1, 2, 3, 4);
      dmx->setFixtureConfig(1, "Fixture 2", 5, 5, 6, 7, 8);
      dmx->setFixtureConfig(2, "Fixture 3", 9, 9, 10, 11, 12);
      dmx->setFixtureConfig(3, "Fixture 4", 13, 13, 14, 15, 16);
      
      Logger::system("DMX fixtures configured. No demo will run automatically.");
      Logger::system("Send a downlink command to control the lights.");
    }
    
    // Ensure all channels are clear at startup
    dmx->clearAllChannels();
    dmx->sendData();
  }
}

void loop() {
  // Ensure all channels are clear at startup
  if (!dmxInitialized) {
    return;
  }
  
  // Handle LoRaWAN events
  if (lora != NULL) {
    lora->handleEvents();
  }
  
  // Check if data was received and process it
  if (dataReceived) {
    // Convert payload to string
    String jsonString = "";
    for (size_t i = 0; i < receivedDataSize; i++) {
      jsonString += (char)receivedData[i];
    }
    
    Logger::loraf("Processing received data on port %d, data: %s", receivedPort, jsonString.c_str());
    
    // Process the received data as JSON
    if (processJsonPayload(jsonString)) {
      Logger::system("Successfully processed downlink payload");
      // Blink LED to indicate successful processing
      DmxController::blinkLED(LED_PIN, 2, 200);
    } else {
      Logger::error("Failed to process downlink payload");
    }
    
    // Reset the data received flag
    dataReceived = false;
  }

  // Handle continuous rainbow demo mode if enabled
  if (runningRainbowDemo && dmxInitialized && dmx != NULL) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastRainbowStep >= rainbowStepDelay) {
      dmx->cycleRainbowStep(rainbowStepCounter++, rainbowStaggered);
      lastRainbowStep = currentMillis;
    }
  }
  
  // Static variables for timing
  static unsigned long lastStatusUpdate = 0;
  static unsigned long lastHeartbeat = 0;
  static unsigned long lastDebugPrint = 0;
  unsigned long currentMillis = millis();
  
  // Print debug information about RX windows every 60 seconds
  if (currentMillis - lastDebugPrint >= 60000) {
    lastDebugPrint = currentMillis;
    
    // Get RX window timing information
    int rx1Delay = lora->getRx1Delay();
    int rx1Timeout = lora->getRx1Timeout();
    int rx2Timeout = lora->getRx2Timeout();
    
    Logger::loraf("\n----- LoRaWAN RX Window Info -----");
    Logger::loraf("RX1 Delay: %d seconds", rx1Delay);
    Logger::loraf("RX1 Window Timeout: %d ms", rx1Timeout);
    Logger::loraf("RX2 Window Timeout: %d ms", rx2Timeout);
    Logger::loraf("----------------------------------\n");
  }
  
  // Send heartbeat ping every 30 seconds to create downlink opportunity
  if (currentMillis - lastHeartbeat >= 30000) {
    lastHeartbeat = currentMillis;
    
    // Only send heartbeat if lora is initialized and joined
    if (lora != NULL && lora->isNetworkJoined()) {
      Logger::lora("Sending heartbeat ping (confirmed uplink)...");
      
      // Send a minimal payload as confirmed uplink
      if (lora->sendString("{\"hb\":1}", 1, true)) {
        Logger::lora("Heartbeat ping sent successfully!");
      } else {
        Logger::error("Failed to send heartbeat ping.");
      }
    }
  }
  
  // Send full status update every 1 minute (reduced from 2 minutes)
  if (currentMillis - lastStatusUpdate >= 60000) {
    lastStatusUpdate = currentMillis;
    
    // Only send status if lora is initialized and joined
    if (lora != NULL && lora->isNetworkJoined()) {
      // Format a status message
      String message = "{\"status\":\"";
      message += dmxInitialized ? "DMX_OK" : "DMX_ERROR";
      message += "\"}";
      
      Logger::system("Sending status update (confirmed uplink)...");
      
      // Send the status message as a confirmed uplink
      if (lora->sendString(message, 1, true)) {
        Logger::system("Status update sent successfully!");
        // Keep the regular interval of 1 minute for the next update
      } else {
        Logger::error("Failed to send status update. Will retry in 30 seconds.");
        // Adjust the last status update time to retry sooner
        lastStatusUpdate = currentMillis - 30000;
      }
    }
  }
  
  // Blink LED occasionally to show we're alive
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 5000) {  // Every 5 seconds
    digitalWrite(LED_PIN, HIGH);
    delay(50);
    digitalWrite(LED_PIN, LOW);
    lastBlink = millis();
  }
  
  // Short delay to prevent hogging the CPU
  delay(10);
}
