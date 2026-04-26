#ifndef AP3216C_H
#define AP3216C_H

#include <Arduino.h>

struct Ap3216cReading {
  bool online = false;
  uint16_t ir = 0;
  uint16_t ps = 0;
  uint16_t als = 0;
};

namespace ap3216c {
bool begin();
bool read(Ap3216cReading *reading);
bool readRegister(uint8_t reg, uint8_t *value);
bool writeRegister(uint8_t reg, uint8_t value);
bool setMode(uint8_t mode);
bool setAlsThreshold(uint16_t low, uint16_t high);
bool setPsThreshold(uint16_t low, uint16_t high);
bool setAlsCalibration(uint8_t value);
bool setPsCalibration(uint16_t value);
bool setIntClearMode(uint8_t mode);
}

#endif
