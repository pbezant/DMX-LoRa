#include "LoRaManager.h"
#include <RadioLib.h>

// Define error codes that are not already defined in RadioLib
// or use the ones from RadioLib directly
// We'll keep these for backward compatibility with existing code
#ifndef RADIOLIB_ERR_INVALID_STATE
#define RADIOLIB_ERR_INVALID_STATE             (-1)
#endif

#ifndef RADIOLIB_ERR_INVALID_INPUT
#define RADIOLIB_ERR_INVALID_INPUT             (-3)
#endif

// Use RadioLib's error codes instead of redefining them
// RADIOLIB_ERR_NETWORK_NOT_JOINED is already defined in RadioLib
// RADIOLIB_ERR_INVALID_FREQUENCY is already defined in RadioLib
// RADIOLIB_LORAWAN_NEW_SESSION is already defined in RadioLib

#ifndef RADIOLIB_ERR_NO_CHANNEL_AVAILABLE
#define RADIOLIB_ERR_NO_CHANNEL_AVAILABLE      (-1106)
#endif

#ifndef RADIOLIB_LORAWAN_NO_DOWNLINK
#define RADIOLIB_LORAWAN_NO_DOWNLINK           (-5)
#endif

// Initialize static instance pointer
LoRaManager* LoRaManager::instance = nullptr;

// Constructor with configurable frequency band and subband
LoRaManager::LoRaManager(LoRaWANBand_t freqBand, uint8_t subBand) : 
  radio(nullptr),
  node(nullptr),
  joinEUI(0),
  devEUI(0),
  freqBand(freqBand),
  subBand(subBand),
  isJoined(false),
  lastRssi(0),
  lastSnr(0),
  receivedBytes(0),
  lastErrorCode(RADIOLIB_ERR_NONE),
  consecutiveTransmitErrors(0) {
  
  // Set this instance as the active one
  instance = this;
  
  // Initialize arrays
  memset(appKey, 0, sizeof(appKey));
  memset(nwkKey, 0, sizeof(nwkKey));
  memset(receivedData, 0, sizeof(receivedData));
  
  // Log selected frequency band using bandNum instead of name
  Serial.print(F("[LoRaManager] Selected frequency band: "));
  Serial.println(freqBand.bandNum);
  
  Serial.print(F("[LoRaManager] Selected subband: "));
  Serial.println(subBand);
}

// Destructor
LoRaManager::~LoRaManager() {
  // Clean up allocated resources
  if (node != nullptr) {
    delete node;
    node = nullptr;
  }
  
  if (radio != nullptr) {
    delete radio;
    radio = nullptr;
  }
  
  // Clear the instance pointer
  if (instance == this) {
    instance = nullptr;
  }
}

// Get band type based on band number
uint8_t LoRaManager::getBandType() {
  // Check band number instead of name
  if (freqBand.bandNum == 0) {
    return BAND_TYPE_OTHER;
  }
  
  // Use band number to determine type
  // US915 is typically band number 2
  if (freqBand.bandNum == 2) {
    return BAND_TYPE_US915;
  } 
  // EU868 is typically band number 1
  else if (freqBand.bandNum == 1) {
    return BAND_TYPE_EU868;
  }
  
  return BAND_TYPE_OTHER;
}

// Initialize the LoRa module
bool LoRaManager::begin(int8_t pinCS, int8_t pinDIO1, int8_t pinReset, int8_t pinBusy) {
  // Store the error code
  lastErrorCode = RADIOLIB_ERR_NONE;
  
  // Create a new Module instance
  Module* module = new Module(pinCS, pinDIO1, pinReset, pinBusy);
  
  // Debug output
  Serial.println(F("[LoRaManager] Creating SX1262 instance..."));
  
  // Create a new SX1262 instance
  radio = new SX1262(module);
  
  // Initialize the radio with more detailed error reporting
  Serial.print(F("[SX1262] Initializing ... "));
  
  // Initialize the radio with more robust error handling
  int state = radio->begin();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    
    // Store the error code
    lastErrorCode = state;
    
    // Additional debug info
    Serial.println(F("[SX1262] Debug info:"));
    Serial.print(F("  CS pin: "));
    Serial.println(pinCS);
    Serial.print(F("  DIO1 pin: "));
    Serial.println(pinDIO1);
    Serial.print(F("  Reset pin: "));
    Serial.println(pinReset);
    Serial.print(F("  Busy pin: "));
    Serial.println(pinBusy);
    
    return false;
  }

  // Log frequency band configuration using band number
  const char* bandName;
  switch(freqBand.bandNum) {
    case 1:
      bandName = "EU868";
      break;
    case 2:
      bandName = "US915";
      break;
    default:
      bandName = "Custom";
  }
  
  Serial.print(F("[LoRaManager] Configuring LoRaWAN for "));
  Serial.print(bandName);
  Serial.println(F(" band..."));
  
  // Initialize the node with the configured region and subband
  // For US915, the subband parameter will automatically configure the correct channels
  node = new LoRaWANNode(radio, &freqBand, subBand);

  // Log detailed band configuration
  Serial.print(F("[LoRaManager] Using "));
  Serial.print(bandName);
  Serial.print(F(" region with subband: "));
  Serial.println(subBand);
  
  // If using US915, log additional information about channels
  uint8_t bandType = getBandType();
  if (bandType == BAND_TYPE_US915) {
    Serial.print(F("[LoRaManager] This will enable channels for subband "));
    Serial.println(subBand);
  }

  // Default values for testing - will be replaced later by setCredentials()
  uint64_t defaultJoinEUI = 0x0000000000000000;
  uint64_t defaultDevEUI = 0x0000000000000000;
  uint8_t defaultNwkKey[16] = {0};
  uint8_t defaultAppKey[16] = {0};
  
  // Initialize node
  Serial.println(F("[LoRaManager] Initializing node..."));
  
  // Initialize with default credentials
  node->beginOTAA(defaultJoinEUI, defaultDevEUI, defaultNwkKey, defaultAppKey);
  
  Serial.println(F("[LoRaManager] LoRaWAN node initialized successfully!"));
  
  return true;
}

// Configure subband channel mask based on the current subband
int LoRaManager::configureSubbandChannels(uint8_t targetSubBand) {
  if (!node) {
    Serial.println(F("[LoRaWAN] Node not initialized"));
    return RADIOLIB_ERR_INVALID_STATE;
  }
  
  // Only applicable for US915
  uint8_t bandType = getBandType();
  if (bandType != BAND_TYPE_US915) {
    Serial.println(F("[LoRaWAN] Subband configuration only applies to US915"));
    return RADIOLIB_ERR_NONE;
  }
  
  // Validate subband (1-8)
  if (targetSubBand < 1 || targetSubBand > 8) {
    Serial.println(F("[LoRaWAN] Invalid subband, must be 1-8"));
    return RADIOLIB_ERR_INVALID_INPUT;
  }
  
  Serial.print(F("[LoRaWAN] Subband configuration is handled automatically during initialization"));
  Serial.print(F(" for subband "));
  Serial.println(targetSubBand);
  
  // In RadioLib 7.1.2, the subband is configured during node initialization
  // and channel selection is handled internally, so we don't need to do anything here
  return RADIOLIB_ERR_NONE;
}

// Set the LoRaWAN credentials
void LoRaManager::setCredentials(uint64_t joinEUI, uint64_t devEUI, uint8_t* appKey, uint8_t* nwkKey) {
  this->joinEUI = joinEUI;
  this->devEUI = devEUI;
  
  // Copy the keys
  memcpy(this->appKey, appKey, 16);
  memcpy(this->nwkKey, nwkKey, 16);
}

// Join the LoRaWAN network
bool LoRaManager::joinNetwork() {
  if (node == nullptr) {
    Serial.println(F("[LoRaWAN] Node not initialized!"));
    lastErrorCode = RADIOLIB_ERR_INVALID_STATE;
    return false;
  }
  
  // Maximum number of join attempts
  const uint8_t maxAttempts = 5;
  uint8_t attemptCount = 0;
  
  // Initial backoff delay in milliseconds
  uint16_t backoffDelay = 1000;
  
  // Try multiple times with exponential backoff
  while (attemptCount < maxAttempts) {
    // Increment attempt counter
    attemptCount++;
    
    // Attempt to join the network
    Serial.print(F("[LoRaWAN] Attempting over-the-air activation (attempt "));
    Serial.print(attemptCount);
    Serial.print(F(" of "));
    Serial.print(maxAttempts);
    Serial.print(F(") ... "));
    
    // Set the proper credentials before activation
    node->beginOTAA(joinEUI, devEUI, nwkKey, appKey);
    
    // Select a subband based on the attempt number
    uint8_t currentSubBand = attemptCount == 1 ? subBand : (1 + (attemptCount % 8)); // Start with configured subband, then try others
    
    // Configure channels for the selected subband (US915 only)
    uint8_t bandType = getBandType();
    if (bandType == BAND_TYPE_US915) {
      int maskResult = configureSubbandChannels(currentSubBand);
      
      // If we couldn't set the channel mask, try the next attempt
      if (maskResult != RADIOLIB_ERR_NONE) {
        Serial.println(F("[LoRaWAN] Continuing with default channel configuration"));
      }
    }

    // Try to join the network
    int state = node->activateOTAA();
    lastErrorCode = state;
    
    // Check for successful join or new session status
    if (state == RADIOLIB_ERR_NONE || state == RADIOLIB_LORAWAN_NEW_SESSION) {
      if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("success!"));
      } else {
        Serial.println(F("success! (new session started)"));
      }
      isJoined = true;
      
      // Set a lower data rate for more reliable transmission
      // For US915, DR0 is SF10/125kHz which is more reliable than higher DRs
      if (bandType == BAND_TYPE_US915) {
        Serial.println(F("[LoRaWAN] Setting data rate to DR0 for reliability"));
        node->setDatarate(0);
      } else {
        // For other bands like EU868, DR1 (SF9/125kHz) is a good balance
        Serial.println(F("[LoRaWAN] Setting data rate to DR1 for reliability"));
        node->setDatarate(1);
      }
      
      return true;
    } else {
      Serial.print(F("failed, code "));
      Serial.println(state);
      
      // If we haven't reached the maximum number of attempts, wait and retry
      if (attemptCount < maxAttempts) {
        Serial.print(F("[LoRaWAN] Retrying in "));
        Serial.print(backoffDelay / 1000.0);
        Serial.println(F(" seconds..."));
        
        // Wait with exponential backoff delay
        delay(backoffDelay);
        
        // Increase backoff delay for next attempt (exponential backoff)
        backoffDelay *= 2;
      }
    }
  }
  
  // If we've reached this point, all join attempts failed
  Serial.println(F("[LoRaWAN] All join attempts failed. Please check your configuration and signal."));
  isJoined = false;
  return false;
}

// Send data to the LoRaWAN network
bool LoRaManager::sendData(uint8_t* data, size_t len, uint8_t port, bool confirmed) {
  // Maximum number of transmission attempts
  const uint8_t maxAttempts = 3;
  uint8_t attemptCount = 0;
  
  if (!isJoined || node == nullptr) {
    Serial.println(F("[LoRaWAN] Not joined to network!"));
    
    // Add automatic rejoin attempt when trying to send while not joined
    Serial.println(F("[LoRaWAN] Attempting to rejoin the network..."));
    if (joinNetwork()) {
      Serial.println(F("[LoRaWAN] Successfully rejoined, will now try to send data"));
    } else {
      Serial.println(F("[LoRaWAN] Rejoin failed, cannot send data"));
      return false;
    }
  }
  
  // Retry loop for sending data
  while (attemptCount < maxAttempts) {
    // Increment attempt counter
    attemptCount++;
    
    // Send the data
    Serial.print(F("[LoRaWAN] Sending data (attempt "));
    Serial.print(attemptCount);
    Serial.print(F(" of "));
    Serial.print(maxAttempts);
    Serial.print(F(") ... "));
    
    // Prepare buffer for downlink
    uint8_t downlinkData[256];
    size_t downlinkLen = sizeof(downlinkData);
    
    // Send data and wait for downlink
    int state = node->sendReceive(data, len, port, downlinkData, &downlinkLen, confirmed);
    lastErrorCode = state;
    
    // Check for successful transmission
    if (state == RADIOLIB_ERR_NONE || state > 0 || state == RADIOLIB_LORAWAN_NO_DOWNLINK) {
      if (state > 0) {
        // Downlink received in window state (1 = RX1, 2 = RX2)
        Serial.print(F("success! Received downlink in RX"));
        Serial.println(state);
        
        // Process the downlink data
        if (downlinkLen > 0) {
          Serial.print(F("[LoRaWAN] Received "));
          Serial.print(downlinkLen);
          Serial.println(F(" bytes:"));
          
          for (size_t i = 0; i < downlinkLen; i++) {
            Serial.print(downlinkData[i], HEX);
            Serial.print(' ');
          }
          Serial.println();
          
          // Copy the data to our buffer
          memcpy(receivedData, downlinkData, downlinkLen);
          receivedBytes = downlinkLen;
        }
      } else if (state == RADIOLIB_LORAWAN_NO_DOWNLINK) {
        // No downlink received but uplink was successful
        Serial.println(F("success! No downlink received."));
      } else {
        // General success
        Serial.println(F("success!"));
      }
      
      // Get RSSI and SNR
      lastRssi = radio->getRSSI();
      lastSnr = radio->getSNR();
      
      consecutiveTransmitErrors = 0; // Reset error counter on success
      return true;
    } else {
      // Error occurred
      Serial.print(F("failed, code "));
      Serial.println(state);
      
      // Add more specific error handling for common LoRaWAN transmission issues
      bool shouldRetry = false;
      
      // Handle different error cases
      if (state == RADIOLIB_ERR_TX_TIMEOUT) {
        Serial.println(F("[LoRaWAN] Transmission timeout. Check antenna and signal."));
        shouldRetry = true;
      } 
      else if (state == RADIOLIB_ERR_NETWORK_NOT_JOINED) {
        Serial.println(F("[LoRaWAN] Network not joined. Will try to rejoin."));
        isJoined = false; // Force rejoin
        
        // Try to rejoin before next attempt
        if (joinNetwork()) {
          Serial.println(F("[LoRaWAN] Rejoined successfully, will retry transmission."));
          shouldRetry = true;
        } else {
          Serial.println(F("[LoRaWAN] Failed to rejoin, cannot continue."));
          shouldRetry = false;
        }
      }
      else if (state == RADIOLIB_ERR_NO_CHANNEL_AVAILABLE) {
        Serial.println(F("[LoRaWAN] No channel available for the requested data rate."));
        
        // Only try different subbands for US915
        uint8_t bandType = getBandType();
        if (bandType == BAND_TYPE_US915) {
          // Try selecting a different subband for next attempt
          uint8_t alternateSubBand = 1 + (attemptCount % 8); // Try different subbands (1-8)
          Serial.print(F("[LoRaWAN] Will try with subband "));
          Serial.print(alternateSubBand);
          Serial.println(F(" for next attempt"));
          
          // Configure the alternate subband
          int maskResult = configureSubbandChannels(alternateSubBand);
          
          if (maskResult == RADIOLIB_ERR_NONE) {
            shouldRetry = true;
          } else {
            shouldRetry = false;
          }
        } else {
          Serial.println(F("[LoRaWAN] Subband adjustment not applicable for this region"));
          shouldRetry = (attemptCount < maxAttempts);
        }
      }
      else {
        // Default case for other errors
        Serial.println(F("[LoRaWAN] Unknown error during transmission."));
        shouldRetry = (attemptCount < maxAttempts);
      }
      
      // Track consecutive errors
      consecutiveTransmitErrors++;
      
      // If we should retry and have attempts left
      if (shouldRetry && attemptCount < maxAttempts) {
        Serial.print(F("[LoRaWAN] Will retry transmission in 3 seconds (attempt "));
        Serial.print(attemptCount + 1);
        Serial.print(F(" of "));
        Serial.print(maxAttempts);
        Serial.println(F(")"));
        
        // Wait before next attempt
        delay(3000);
      } else {
        // If we've encountered errors multiple times in a row, try rejoining on next transmission
        if (consecutiveTransmitErrors >= 3) {
          Serial.println(F("[LoRaWAN] Multiple transmission errors, will attempt to rejoin on next transmission."));
          isJoined = false; // Force a rejoin on next transmission
        }
        
        return false;
      }
    }
  }
  
  // If we've reached this point, all attempts failed
  Serial.println(F("[LoRaWAN] All transmission attempts failed."));
  return false;
}

// Helper method to send a string
bool LoRaManager::sendString(const String& data, uint8_t port) {
  return sendData((uint8_t*)data.c_str(), data.length(), port);
}

// Get the last RSSI value
float LoRaManager::getLastRssi() {
  return lastRssi;
}

// Get the last SNR value
float LoRaManager::getLastSnr() {
  return lastSnr;
}

// Check if the device is joined to the network
bool LoRaManager::isNetworkJoined() {
  return isJoined;
}

// Handle events (should be called in the loop)
void LoRaManager::handleEvents() {
  // This method is kept for compatibility but is not needed with the current implementation
  // as the sendReceive method already handles downlink reception
}

// Get the last error from LoRaWAN operations
int LoRaManager::getLastErrorCode() {
  return lastErrorCode;
} 