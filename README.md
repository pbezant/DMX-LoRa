# DMX LoRa Control System

This project implements a DMX lighting controller using a Heltec LoRa 32 V3 microcontroller that receives control commands over The Things Network (TTN) via LoRaWAN. It allows remote control of DMX lighting fixtures through JSON-formatted messages.

## Features

- Connects to The Things Network (TTN) using LoRaWAN (US915 frequency plan)
- Uses OTAA (Over-The-Air Activation) for secure network joining
- Receives JSON-formatted commands for controlling DMX fixtures
- Processes JSON payloads to control multiple DMX fixtures at different addresses
- Includes comprehensive error handling and debugging features
- Supports dynamic fixture configuration without hardcoded settings

## Hardware Requirements

- Heltec LoRa 32 V3 microcontroller
- MAX485 transceiver for DMX output
- 120 ohm terminating resistor for the DMX line
- DMX lighting fixtures

## Wiring Diagram

Connect the MAX485 transceiver to the Heltec LoRa 32 V3 as follows:

| Heltec Pin | MAX485 Pin | Function |
|------------|------------|----------|
| 19 (TX)    | DI         | Data Input (transmit data to DMX) |
| 20 (RX)    | RO         | Data Output (receive data from DMX if needed) |
| 5          | DE & RE    | Direction control (connect to both DE and RE) |
| 3.3V       | VCC        | Power supply |
| GND        | GND        | Ground |

Connect the DMX output from the MAX485 as follows:

| MAX485 Pin | DMX Pin   | Notes |
|------------|-----------|-------|
| A          | DMX+ (3)  | Data+ |
| B          | DMX- (2)  | Data- |
| GND        | GND (1)   | Ground (optional depending on setup) |

Don't forget to add a 120 ohm resistor between A and B at the end of the DMX line for proper termination.

## Software Setup

### Required Libraries

This project uses the following libraries:
- RadioLib (for LoRaWAN communication)
- ArduinoJson (for JSON parsing)
- esp_dmx (for DMX control)

Plus the custom libraries included in the project:
- LoRaManager (a wrapper around RadioLib for easier LoRaWAN management)
- DmxController (a wrapper around esp_dmx for easier DMX control)

### PlatformIO Configuration

The project uses PlatformIO for dependency management and building. The configuration is in the `platformio.ini` file.

### TTN Configuration

1. Create an application in The Things Network Console
2. Register your device in the application (using OTAA)
3. Update the credentials in the code:
   - joinEUI (Application EUI)
   - devEUI (Device EUI)
   - appKey (Application Key)

### Modifying LoRaWAN Credentials

You can edit the LoRaWAN credentials in the main.cpp file:

```cpp
// LoRaWAN Credentials (can be changed by the user)
uint64_t joinEUI = 0x0000000000000001; // Replace with your Application EUI
uint64_t devEUI = 0x70B3D57ED80041B2;  // Replace with your Device EUI
uint8_t appKey[] = {0x45, 0xD3, 0x7B, 0xF3, 0x77, 0x61, 0xA6, 0x1F, 0x9F, 0x07, 0x1F, 0xE1, 0x6D, 0x4F, 0x57, 0x77}; // Replace with your Application Key
```

## JSON Command Format

The system expects JSON commands in the following format:

```json
{
  "lights": [
    {
      "address": 1,
      "channels": [255, 0, 128, 0]
    },
    {
      "address": 5,
      "channels": [255, 255, 100, 0]
    }
  ]
}
```

Where:
- `address`: The DMX start address of the fixture (1-512)
- `channels`: An array of DMX channel values (0-255) for each channel, relative to the start address

## TTN Payload Formatter

Add the following JavaScript decoder in your TTN application to format the downlink payload as a JSON string:

```javascript
function decodeDownlink(input) {
  // For downlink, we simply pass through the JSON string
  try {
    var jsonString = String.fromCharCode.apply(null, input.bytes);
    return {
      data: {
        jsonData: jsonString
      },
      warnings: [],
      errors: []
    };
  } catch (error) {
    return {
      data: {},
      warnings: [],
      errors: ["Failed to process downlink: " + error]
    };
  }
}

function encodeDownlink(input) {
  // Convert the input JSON object to a string
  var jsonString = JSON.stringify(input.data);
  
  // Convert the string to an array of bytes
  var bytes = [];
  for (var i = 0; i < jsonString.length; i++) {
    bytes.push(jsonString.charCodeAt(i));
  }
  
  return {
    bytes: bytes,
    fPort: 1
  };
}
```

## Troubleshooting

### LED Blink Codes

The onboard LED will blink to indicate different states:
- 2 blinks (slow): Device started successfully
- 3 blinks: Successfully joined TTN
- 4 blinks: Failed to join TTN
- 5 blinks (rapid): Error in initialization or JSON processing

### Serial Output

Connect to the serial monitor at 115200 baud to see detailed diagnostic information.

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Credits

- Heltec for the LoRa 32 V3 hardware
- The Things Network for LoRaWAN infrastructure
- Contributors to the used libraries 

### Example Commands

1. **Ping Test Command**
   ```json
   {
     "test": {
       "pattern": "ping"
     }
   }
   ```
   - Purpose: Test downlink communication
   - Expected Response: `{"ping_response":"ok"}`
   - Device Action: Blinks LED 3 times

2. **Rainbow Chase Test Pattern**
   ```json
   {
     "test": {
       "pattern": "rainbow",
       "cycles": 3,
       "speed": 50,
       "staggered": true
     }
   }
   ```
   - Purpose: Run a rainbow chase pattern across all fixtures
   - Optional Parameters:
     - `cycles`: Number of color cycles (1-10, default: 3)
     - `speed`: Milliseconds between updates (10-500, default: 50)
     - `staggered`: Create chase effect across fixtures (default: true)
   - Expected Response: `{"status":"DMX_OK"}`

3. **Strobe Test Pattern**
   ```json
   {
     "test": {
       "pattern": "strobe",
       "color": 1,
       "count": 20,
       "onTime": 50,
       "offTime": 50,
       "alternate": false
     }
   }
   ```
   - Purpose: Flash lights in a strobe pattern
   - Optional Parameters:
     - `color`: 0=white, 1=red, 2=green, 3=blue (default: 0)
     - `count`: Number of flashes (1-100, default: 20)
     - `onTime`: Milliseconds for on phase (10-1000, default: 50)
     - `offTime`: Milliseconds for off phase (10-1000, default: 50)
     - `alternate`: Alternate between fixtures (default: false)
   - Expected Response: `{"status":"DMX_OK"}`

4. **Continuous Rainbow Mode**
   ```json
   {
     "test": {
       "pattern": "continuous",
       "enabled": true,
       "speed": 30,
       "staggered": true
     }
   }
   ```
   - Purpose: Enable/disable continuous rainbow effect
   - Optional Parameters:
     - `enabled`: true to enable, false to disable (default: false)
     - `speed`: Milliseconds between updates (5-500, default: 30)
     - `staggered`: Create chase effect across fixtures (default: true)
   - Expected Response: `{"status":"DMX_OK"}`

5. **DMX Control - Full Brightness**
   ```json
   {
     "lights": [
       {
         "address": 1,
         "channels": [255, 255, 255, 255, 255, 255, 255, 255]
       }
     ]
   }
   ```
   - Purpose: Set all channels of a light fixture to maximum brightness
   - Expected Response: `{"status":"DMX_OK"}`

6. **DMX Control - RGB Example (Blue and Green)**
   ```json
   {
     "lights": [
       {
         "address": 1,
         "channels": [0, 0, 255, 0, 0, 0, 0, 0]
       },
       {
         "address": 2,
         "channels": [0, 255, 0, 0, 0, 0, 0, 0]
       }
     ]
   }
   ```
   - Purpose: Set fixture 1 to blue and fixture 2 to green
   - Expected Response: `{"status":"DMX_OK"}`

7. **DMX Control - Multiple Fixtures**
   ```json
   {
     "lights": [
       {
         "address": 1,
         "channels": [255, 255, 255, 0, 0, 0, 0, 0]
       },
       {
         "address": 2,
         "channels": [0, 0, 0, 255, 255, 255, 0, 0]
       }
     ]
   }
   ```
   - Purpose: Control multiple fixtures with different channel values
   - Expected Response: `{"status":"DMX_OK"}`

## Logging System

The system includes a categorized logging system that allows you to filter serial output based on different message types. This makes debugging easier by focusing on specific aspects of the system.

### Log Categories

- **SYSTEM**: System startup, initialization, and general status messages
- **DMX**: DMX-related operations and lighting control
- **LORA**: LoRa/LoRaWAN communication messages
- **JSON**: JSON parsing and processing
- **TEST**: Test pattern execution logs
- **DEBUG**: Detailed debug information
- **ERROR**: Error messages

### Controlling Log Output

To enable or disable specific log categories, you can modify the code in `setup()`:

```cpp
// Enable all categories
Logger::begin(115200);

// Enable only specific categories
Logger::begin(115200, LOG_CATEGORY_SYSTEM | LOG_CATEGORY_ERROR);

// After initialization, you can change categories
Logger::enableCategory(LOG_CATEGORY_DMX);
Logger::disableCategory(LOG_CATEGORY_DEBUG);
```

### Viewing Logs

Connect to the serial monitor at 115200 baud to see the categorized log output. Each message will be prefixed with its category:

```
[SYSTEM] DMX LoRa Control System initialized
[LORA] Joining LoRaWAN network...
[DMX] Setting channels for fixture at address 1: 255 0 255 0
[ERROR] Failed to parse JSON payload
```

This makes it easier to identify the source of messages and troubleshoot specific components. 