#ifndef STORAGE_H
#define STORAGE_H

#include "config.h"

// External dependencies
extern CoffeeConfig coffeeConfig;

// Initialize preferences/storage
void initStorage();

// Save configuration to flash memory
void saveConfiguration();

// Load configuration from flash memory
void loadConfiguration();

#endif // STORAGE_H

