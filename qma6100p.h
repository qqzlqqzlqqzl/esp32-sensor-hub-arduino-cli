#ifndef QMA6100P_H
#define QMA6100P_H

#include <Arduino.h>

struct Qma6100pReading {
  bool online = false;
  float ax = 0.0f;
  float ay = 0.0f;
  float az = 0.0f;
  float ag = 0.0f;
  float pitch = 0.0f;
  float roll = 0.0f;
};

namespace qma6100p {
bool begin();
bool read(Qma6100pReading *reading);
bool readRegister(uint8_t reg, uint8_t *value);
bool writeRegister(uint8_t reg, uint8_t value);
bool setRangeG(uint8_t rangeG);
uint8_t currentRangeG();
bool setBandwidth(uint8_t bandwidth);
bool setPowerMode(uint8_t value);
bool setInterruptLatch(bool enabled);
bool setStepInterrupt(bool enabled, uint8_t map);
bool setTapPreset(uint8_t preset);
bool setMotionPreset(uint8_t preset);
}

#endif
