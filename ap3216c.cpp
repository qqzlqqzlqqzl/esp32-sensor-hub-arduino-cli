#include "ap3216c.h"

#include <Wire.h>

namespace {
constexpr uint8_t kAddress = 0x1E;

bool writeReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(kAddress);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool readReg(uint8_t reg, uint8_t *value) {
  Wire.beginTransmission(kAddress);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  if (Wire.requestFrom(static_cast<int>(kAddress), 1) != 1 || !Wire.available()) {
    return false;
  }

  *value = Wire.read();
  return true;
}
}  // namespace

namespace ap3216c {

bool begin() {
  uint8_t value = 0;
  if (!writeReg(0x00, 0x04)) {
    return false;
  }
  delay(50);
  if (!writeReg(0x00, 0x03)) {
    return false;
  }
  if (!readReg(0x00, &value)) {
    return false;
  }
  return value == 0x03;
}

bool read(Ap3216cReading *reading) {
  uint8_t buf[6];

  for (uint8_t i = 0; i < 6; i++) {
    if (!readReg(0x0A + i, &buf[i])) {
      reading->online = false;
      return false;
    }
  }

  reading->online = true;
  reading->ir = (buf[0] & 0x80) ? 0 : static_cast<uint16_t>((buf[1] << 2) | (buf[0] & 0x03));
  reading->als = static_cast<uint16_t>((buf[3] << 8) | buf[2]);
  reading->ps = (buf[4] & 0x40) ? 0 : static_cast<uint16_t>(((buf[5] & 0x3F) << 4) | (buf[4] & 0x0F));
  return true;
}

}  // namespace ap3216c
