#include "Logger.h"

// Initialize static members
uint8_t Logger::_enabledCategories = LOG_CATEGORY_ALL;
bool Logger::_initialized = false;

void Logger::begin(unsigned long baud, uint8_t enabledCategories) {
    if (!_initialized) {
        Serial.begin(baud);
        delay(100); // Short delay for Serial to initialize
        _initialized = true;
    }
    
    _enabledCategories = enabledCategories;
    
    // Print enabled categories
    Serial.println("\n===== Logger Initialized =====");
    Serial.print("Enabled categories: ");
    
    if (_enabledCategories == LOG_CATEGORY_ALL) {
        Serial.println("ALL");
    } else {
        if (_enabledCategories & LOG_CATEGORY_SYSTEM) Serial.print("SYSTEM ");
        if (_enabledCategories & LOG_CATEGORY_DMX) Serial.print("DMX ");
        if (_enabledCategories & LOG_CATEGORY_LORA) Serial.print("LORA ");
        if (_enabledCategories & LOG_CATEGORY_JSON) Serial.print("JSON ");
        if (_enabledCategories & LOG_CATEGORY_TEST) Serial.print("TEST ");
        if (_enabledCategories & LOG_CATEGORY_DEBUG) Serial.print("DEBUG ");
        if (_enabledCategories & LOG_CATEGORY_ERROR) Serial.print("ERROR ");
        Serial.println();
    }
    Serial.println("=============================");
}

void Logger::enableCategory(uint8_t category) {
    _enabledCategories |= category;
}

void Logger::disableCategory(uint8_t category) {
    _enabledCategories &= ~category;
}

void Logger::setCategories(uint8_t categories) {
    _enabledCategories = categories;
}

bool Logger::isCategoryEnabled(uint8_t category) {
    return (_enabledCategories & category) != 0;
}

void Logger::log(uint8_t category, const char* message) {
    if (isCategoryEnabled(category)) {
        Serial.print("[");
        Serial.print(getCategoryName(category));
        Serial.print("] ");
        Serial.println(message);
    }
}

void Logger::log(uint8_t category, const String& message) {
    log(category, message.c_str());
}

void Logger::logf(uint8_t category, const char* format, ...) {
    if (isCategoryEnabled(category)) {
        char buffer[256]; // Adjust size as needed
        
        // Print category prefix
        int prefixLen = snprintf(buffer, sizeof(buffer), "[%s] ", getCategoryName(category));
        
        // Format the rest of the message
        va_list args;
        va_start(args, format);
        vsnprintf(buffer + prefixLen, sizeof(buffer) - prefixLen, format, args);
        va_end(args);
        
        Serial.println(buffer);
    }
}

// Category-specific log methods
void Logger::system(const char* message) {
    log(LOG_CATEGORY_SYSTEM, message);
}

void Logger::dmx(const char* message) {
    log(LOG_CATEGORY_DMX, message);
}

void Logger::lora(const char* message) {
    log(LOG_CATEGORY_LORA, message);
}

void Logger::json(const char* message) {
    log(LOG_CATEGORY_JSON, message);
}

void Logger::test(const char* message) {
    log(LOG_CATEGORY_TEST, message);
}

void Logger::debug(const char* message) {
    log(LOG_CATEGORY_DEBUG, message);
}

void Logger::error(const char* message) {
    log(LOG_CATEGORY_ERROR, message);
}

// Formatted category-specific log methods
void Logger::systemf(const char* format, ...) {
    if (isCategoryEnabled(LOG_CATEGORY_SYSTEM)) {
        char buffer[256]; // Adjust size as needed
        
        // Print category prefix
        int prefixLen = snprintf(buffer, sizeof(buffer), "[SYSTEM] ");
        
        // Format the rest of the message
        va_list args;
        va_start(args, format);
        vsnprintf(buffer + prefixLen, sizeof(buffer) - prefixLen, format, args);
        va_end(args);
        
        Serial.println(buffer);
    }
}

void Logger::dmxf(const char* format, ...) {
    if (isCategoryEnabled(LOG_CATEGORY_DMX)) {
        char buffer[256]; // Adjust size as needed
        
        // Print category prefix
        int prefixLen = snprintf(buffer, sizeof(buffer), "[DMX] ");
        
        // Format the rest of the message
        va_list args;
        va_start(args, format);
        vsnprintf(buffer + prefixLen, sizeof(buffer) - prefixLen, format, args);
        va_end(args);
        
        Serial.println(buffer);
    }
}

void Logger::loraf(const char* format, ...) {
    if (isCategoryEnabled(LOG_CATEGORY_LORA)) {
        char buffer[256]; // Adjust size as needed
        
        // Print category prefix
        int prefixLen = snprintf(buffer, sizeof(buffer), "[LORA] ");
        
        // Format the rest of the message
        va_list args;
        va_start(args, format);
        vsnprintf(buffer + prefixLen, sizeof(buffer) - prefixLen, format, args);
        va_end(args);
        
        Serial.println(buffer);
    }
}

void Logger::jsonf(const char* format, ...) {
    if (isCategoryEnabled(LOG_CATEGORY_JSON)) {
        char buffer[256]; // Adjust size as needed
        
        // Print category prefix
        int prefixLen = snprintf(buffer, sizeof(buffer), "[JSON] ");
        
        // Format the rest of the message
        va_list args;
        va_start(args, format);
        vsnprintf(buffer + prefixLen, sizeof(buffer) - prefixLen, format, args);
        va_end(args);
        
        Serial.println(buffer);
    }
}

void Logger::testf(const char* format, ...) {
    if (isCategoryEnabled(LOG_CATEGORY_TEST)) {
        char buffer[256]; // Adjust size as needed
        
        // Print category prefix
        int prefixLen = snprintf(buffer, sizeof(buffer), "[TEST] ");
        
        // Format the rest of the message
        va_list args;
        va_start(args, format);
        vsnprintf(buffer + prefixLen, sizeof(buffer) - prefixLen, format, args);
        va_end(args);
        
        Serial.println(buffer);
    }
}

void Logger::debugf(const char* format, ...) {
    if (isCategoryEnabled(LOG_CATEGORY_DEBUG)) {
        char buffer[256]; // Adjust size as needed
        
        // Print category prefix
        int prefixLen = snprintf(buffer, sizeof(buffer), "[DEBUG] ");
        
        // Format the rest of the message
        va_list args;
        va_start(args, format);
        vsnprintf(buffer + prefixLen, sizeof(buffer) - prefixLen, format, args);
        va_end(args);
        
        Serial.println(buffer);
    }
}

void Logger::errorf(const char* format, ...) {
    if (isCategoryEnabled(LOG_CATEGORY_ERROR)) {
        char buffer[256]; // Adjust size as needed
        
        // Print category prefix
        int prefixLen = snprintf(buffer, sizeof(buffer), "[ERROR] ");
        
        // Format the rest of the message
        va_list args;
        va_start(args, format);
        vsnprintf(buffer + prefixLen, sizeof(buffer) - prefixLen, format, args);
        va_end(args);
        
        Serial.println(buffer);
    }
}

const char* Logger::getCategoryName(uint8_t category) {
    switch (category) {
        case LOG_CATEGORY_SYSTEM: return "SYSTEM";
        case LOG_CATEGORY_DMX: return "DMX";
        case LOG_CATEGORY_LORA: return "LORA";
        case LOG_CATEGORY_JSON: return "JSON";
        case LOG_CATEGORY_TEST: return "TEST";
        case LOG_CATEGORY_DEBUG: return "DEBUG";
        case LOG_CATEGORY_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
} 