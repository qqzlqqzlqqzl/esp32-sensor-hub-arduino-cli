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
}

#endif
