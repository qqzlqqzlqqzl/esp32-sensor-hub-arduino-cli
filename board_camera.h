#ifndef BOARD_CAMERA_H
#define BOARD_CAMERA_H

#include <Arduino.h>
#include <esp_camera.h>

struct BoardCameraStatus {
  bool online = false;
  bool psram = false;
  bool externalClock = true;
  int initError = 0;
  uint16_t pid = 0;
  char name[16] = "none";
  int frameSize = 0;
  int quality = 0;
  int brightness = 0;
  int contrast = 0;
  int saturation = 0;
  int sharpness = 0;
  int specialEffect = 0;
  int wbMode = 0;
  int awb = 0;
  int awbGain = 0;
  int aec = 0;
  int aec2 = 0;
  int aeLevel = 0;
  int aecValue = 0;
  int agc = 0;
  int agcGain = 0;
  int gainCeiling = 0;
  int hmirror = 0;
  int vflip = 0;
  int colorbar = 0;
  uint32_t captureCount = 0;
  uint32_t captureFailures = 0;
  uint32_t consecutiveCaptureFailures = 0;
  uint32_t recoveryCount = 0;
  uint32_t lastRecoveryMs = 0;
  uint32_t lastCaptureMs = 0;
  uint32_t lastCaptureDurationMs = 0;
  uint32_t lastFrameBytes = 0;
  uint16_t lastWidth = 0;
  uint16_t lastHeight = 0;
};

struct BoardCameraControl {
  const char *name;
  int value;
  BoardCameraControl(const char *controlName, int controlValue)
      : name(controlName), value(controlValue) {}
};

namespace boardcamera {
bool begin();
bool isReady();
const BoardCameraStatus &status();
camera_fb_t *capture();
void release(camera_fb_t *frame);
bool setControl(const String &name, int value);
bool setControls(const BoardCameraControl *controls, size_t count, size_t *appliedCount);
bool readRegister(uint16_t reg, uint8_t mask, int *value);
bool writeRegister(uint16_t reg, uint8_t mask, uint8_t value);
const char *frameSizeName(int frameSize);
int frameSizeFromName(const String &name);
bool isSupportedFrameSize(int frameSize);
}

#endif
