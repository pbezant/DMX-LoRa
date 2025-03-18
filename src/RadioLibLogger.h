#ifndef RADIOLIB_LOGGER_H
#define RADIOLIB_LOGGER_H

#include "Logger.h"

// Redefine RadioLib debug printing to use our Logger
#undef RADIOLIB_DEBUG_PORT
#define RADIOLIB_DEBUG_PORT CustomRadioLibDebugPort

// Custom debug port class that redirects to Logger
class CustomRadioLibDebugPortClass {
public:
    size_t print(const char* str) {
        Logger::radiolib(str);
        return strlen(str);
    }
    
    size_t print(unsigned long num) {
        char buf[16];
        sprintf(buf, "%lu", num);
        Logger::radiolib(buf);
        return strlen(buf);
    }
    
    size_t print(unsigned long num, int base) {
        char buf[16];
        ltoa(num, buf, base);
        Logger::radiolib(buf);
        return strlen(buf);
    }
    
    size_t print(double num, int decimals) {
        char buf[32];
        dtostrf(num, 0, decimals, buf);
        Logger::radiolib(buf);
        return strlen(buf);
    }
    
    size_t println(const char* str = "") {
        Logger::radiolib(str);
        return strlen(str);
    }
    
    size_t println(unsigned long num) {
        char buf[16];
        sprintf(buf, "%lu", num);
        Logger::radiolib(buf);
        return strlen(buf);
    }
    
    size_t write(const uint8_t* buf, size_t len) {
        // This is a simplified version - convert binary data to string
        char str[512];  // Limit the length to avoid buffer overflow
        if(len > 250) len = 250;  // Safety limit
        
        // Create a copy of the data as a null-terminated string
        memcpy(str, buf, len);
        str[len] = '\0';
        
        Logger::radiolib(str);
        return len;
    }
};

// Create a global instance
extern CustomRadioLibDebugPortClass CustomRadioLibDebugPort;

#endif // RADIOLIB_LOGGER_H 