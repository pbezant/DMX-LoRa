#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

// Log categories
#define LOG_CATEGORY_SYSTEM     0x01  // System startup, initialization, etc.
#define LOG_CATEGORY_DMX        0x02  // DMX-related operations
#define LOG_CATEGORY_LORA       0x04  // LoRa/LoRaWAN communication
#define LOG_CATEGORY_JSON       0x08  // JSON parsing and processing
#define LOG_CATEGORY_TEST       0x10  // Test patterns execution
#define LOG_CATEGORY_DEBUG      0x20  // Detailed debug information
#define LOG_CATEGORY_ERROR      0x40  // Error messages
#define LOG_CATEGORY_RADIOLIB   0x80  // RadioLib debug output (RLB_PRO, etc.)
#define LOG_CATEGORY_ALL        0xFF  // All categories

class Logger {
public:
    // Initialize logger with default enabled categories
    static void begin(unsigned long baud = 115200, uint8_t enabledCategories = LOG_CATEGORY_ALL);
    
    // Enable/disable specific log categories
    static void enableCategory(uint8_t category);
    static void disableCategory(uint8_t category);
    static void setCategories(uint8_t categories);
    
    // Check if a category is enabled
    static bool isCategoryEnabled(uint8_t category);
    
    // Log methods with category filtering
    static void log(uint8_t category, const char* message);
    static void log(uint8_t category, const String& message);
    static void logf(uint8_t category, const char* format, ...);
    
    // Category-specific log methods
    static void system(const char* message);
    static void dmx(const char* message);
    static void lora(const char* message);
    static void json(const char* message);
    static void test(const char* message);
    static void debug(const char* message);
    static void error(const char* message);
    static void radiolib(const char* message);
    
    // Formatted category-specific log methods
    static void systemf(const char* format, ...);
    static void dmxf(const char* format, ...);
    static void loraf(const char* format, ...);
    static void jsonf(const char* format, ...);
    static void testf(const char* format, ...);
    static void debugf(const char* format, ...);
    static void errorf(const char* format, ...);
    static void radiolibf(const char* format, ...);

private:
    static uint8_t _enabledCategories;
    static bool _initialized;
    static const char* getCategoryName(uint8_t category);
};

#endif // LOGGER_H 