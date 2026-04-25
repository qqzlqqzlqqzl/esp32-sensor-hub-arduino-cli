#include "qma6100p.h"

#include <Wire.h>

#include <math.h>

namespace {
constexpr uint8_t kAddress = 0x12;
constexpr float kGravity = 9.80665f;
constexpr float kRadToDeg = 57.295779513f;
uint8_t gRangeG = 8;

bool writeReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(kAddress);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool readReg(uint8_t reg, uint8_t *buffer, size_t len) {
  Wire.beginTransmission(kAddress);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  if (Wire.requestFrom(static_cast<int>(kAddress), static_cast<int>(len)) != static_cast<int>(len)) {
    return false;
  }

  for (size_t i = 0; i < len; i++) {
    if (!Wire.available()) {
      return false;
    }
    buffer[i] = Wire.read();
  }
  return true;
}
}  // namespace

namespace qma6100p {

bool setRangeG(uint8_t rangeG) {
  uint8_t regValue = 0;
  switch (rangeG) {
    case 2:
      regValue = 0x01;
      break;
    case 4:
      regValue = 0x02;
      break;
    case 8:
      regValue = 0x04;
      break;
    case 16:
      regValue = 0x08;
      break;
    default:
      return false;
  }

  if (!writeReg(0x0F, regValue)) {
    return false;
  }
  gRangeG = rangeG;
  return true;
}

uint8_t currentRangeG() {
  return gRangeG;
}

bool begin() {
  uint8_t id = 0;
  if (!readReg(0x00, &id, 1)) {
    return false;
  }

  writeReg(0x36, 0xB6);
  delay(5);
  writeReg(0x36, 0x00);
  delay(10);

  if (!readReg(0x00, &id, 1) || id != 0x90) {
    return false;
  }

  writeReg(0x11, 0x80);
  writeReg(0x11, 0x84);
  writeReg(0x4A, 0x20);
  writeReg(0x56, 0x01);
  writeReg(0x5F, 0x80);
  delay(1);
  writeReg(0x5F, 0x00);
  delay(10);
  setRangeG(8);
  writeReg(0x10, 0x00);
  writeReg(0x11, 0x84);
  writeReg(0x21, 0x03);
  return true;
}

bool read(Qma6100pReading *reading) {
  uint8_t xyz[6];
  if (!readReg(0x01, xyz, sizeof(xyz))) {
    reading->online = false;
    return false;
  }

  const int16_t rawX = static_cast<int16_t>((xyz[1] << 8) | xyz[0]);
  const int16_t rawY = static_cast<int16_t>((xyz[3] << 8) | xyz[2]);
  const int16_t rawZ = static_cast<int16_t>((xyz[5] << 8) | xyz[4]);
  const float lsbPerG = 8192.0f / static_cast<float>(gRangeG);

  reading->ax = static_cast<float>((rawX >> 2) * kGravity) / lsbPerG;
  reading->ay = static_cast<float>((rawY >> 2) * kGravity) / lsbPerG;
  reading->az = static_cast<float>((rawZ >> 2) * kGravity) / lsbPerG;
  reading->ag = sqrtf(reading->ax * reading->ax + reading->ay * reading->ay + reading->az * reading->az);

  const float normal = reading->ag > 0.001f ? reading->ag : 0.001f;
  const float nx = reading->ax / normal;
  const float ny = reading->ay / normal;
  const float nz = reading->az / normal;

  reading->pitch = -atan2f(reading->ax, reading->az) * kRadToDeg;
  reading->roll = asinf(ny / sqrtf(nx * nx + ny * ny + nz * nz)) * kRadToDeg;
  reading->online = true;
  return true;
}

bool readRegister(uint8_t reg, uint8_t *value) {
  return readReg(reg, value, 1);
}

bool writeRegister(uint8_t reg, uint8_t value) {
  return writeReg(reg, value);
}

}  // namespace qma6100p
