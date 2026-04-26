#include "board_camera.h"

#include "xl9555.h"

#include <freertos/semphr.h>
#include <string.h>

namespace {

BoardCameraStatus gCamera;
SemaphoreHandle_t gCameraMutex = nullptr;
bool gRecoverAfterRelease = false;
volatile bool gControlPending = false;
constexpr uint32_t kCaptureRecoveryTimeoutMs = 3500;
constexpr uint32_t kCaptureRecoveryFailureThreshold = 2;
constexpr TickType_t kCameraControlLockTimeout = pdMS_TO_TICKS(1800);
constexpr size_t kMaxDesiredControls = 24;

struct DesiredCameraControl {
  char name[20] = "";
  int value = 0;
};

DesiredCameraControl gDesiredControls[kMaxDesiredControls];
size_t gDesiredControlCount = 0;

void reapplyDesiredControlsUnlocked(sensor_t *sensor);

constexpr int kPinD0 = 4;
constexpr int kPinD1 = 5;
constexpr int kPinD2 = 6;
constexpr int kPinD3 = 7;
constexpr int kPinD4 = 15;
constexpr int kPinD5 = 16;
constexpr int kPinD6 = 17;
constexpr int kPinD7 = 18;
constexpr int kPinVsync = 47;
constexpr int kPinHref = 48;
constexpr int kPinPclk = 45;
constexpr int kPinSccbScl = 38;
constexpr int kPinSccbSda = 39;

void resetModule() {
  xl9555_io_config(OV_PWDN, IO_SET_OUTPUT);
  xl9555_io_config(OV_RESET, IO_SET_OUTPUT);
  xl9555_pin_set(OV_PWDN, IO_SET_LOW);
  xl9555_pin_set(OV_RESET, IO_SET_LOW);
  delay(10);
  xl9555_pin_set(OV_RESET, IO_SET_HIGH);
  delay(20);
}

void refreshStatusFromSensor() {
  sensor_t *sensor = esp_camera_sensor_get();
  if (!sensor) {
    gCamera.online = false;
    return;
  }

  gCamera.online = true;
  gCamera.pid = sensor->id.PID;
  const camera_sensor_info_t *info = esp_camera_sensor_get_info(&sensor->id);
  snprintf(gCamera.name, sizeof(gCamera.name), "%s", info && info->name ? info->name : "camera");
  gCamera.frameSize = sensor->status.framesize;
  gCamera.quality = sensor->status.quality;
  gCamera.brightness = sensor->status.brightness;
  gCamera.contrast = sensor->status.contrast;
  gCamera.saturation = sensor->status.saturation;
  gCamera.sharpness = sensor->status.sharpness;
  gCamera.specialEffect = sensor->status.special_effect;
  gCamera.wbMode = sensor->status.wb_mode;
  gCamera.awb = sensor->status.awb;
  gCamera.awbGain = sensor->status.awb_gain;
  gCamera.aec = sensor->status.aec;
  gCamera.aec2 = sensor->status.aec2;
  gCamera.aeLevel = sensor->status.ae_level;
  gCamera.aecValue = sensor->status.aec_value;
  gCamera.agc = sensor->status.agc;
  gCamera.agcGain = sensor->status.agc_gain;
  gCamera.gainCeiling = sensor->status.gainceiling;
  gCamera.hmirror = sensor->status.hmirror;
  gCamera.vflip = sensor->status.vflip;
  gCamera.colorbar = sensor->status.colorbar;
}

bool isFrameSizeSupported(int frameSize) {
  return frameSize == FRAMESIZE_QQVGA ||
         frameSize == FRAMESIZE_QVGA ||
         frameSize == FRAMESIZE_VGA ||
         frameSize == FRAMESIZE_SVGA ||
         frameSize == FRAMESIZE_XGA;
}

void fillCameraConfig(camera_config_t *config) {
  memset(config, 0, sizeof(camera_config_t));
  config->pin_pwdn = -1;
  config->pin_reset = -1;
  config->pin_xclk = -1;
  config->pin_sccb_sda = kPinSccbSda;
  config->pin_sccb_scl = kPinSccbScl;
  config->pin_d7 = kPinD7;
  config->pin_d6 = kPinD6;
  config->pin_d5 = kPinD5;
  config->pin_d4 = kPinD4;
  config->pin_d3 = kPinD3;
  config->pin_d2 = kPinD2;
  config->pin_d1 = kPinD1;
  config->pin_d0 = kPinD0;
  config->pin_vsync = kPinVsync;
  config->pin_href = kPinHref;
  config->pin_pclk = kPinPclk;
  config->xclk_freq_hz = 20000000;
  config->ledc_timer = LEDC_TIMER_1;
  config->ledc_channel = LEDC_CHANNEL_1;
  config->pixel_format = PIXFORMAT_JPEG;
  config->frame_size = FRAMESIZE_QQVGA;
  config->jpeg_quality = 30;
  config->fb_count = gCamera.psram ? 3 : 1;
  config->fb_location = gCamera.psram ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
  config->grab_mode = CAMERA_GRAB_LATEST;
  config->sccb_i2c_port = 1;
}

bool initCameraLocked() {
  gCamera.psram = psramFound();
  gCamera.externalClock = true;
  resetModule();

  camera_config_t config;
  fillCameraConfig(&config);
  const esp_err_t err = esp_camera_init(&config);
  gCamera.initError = static_cast<int>(err);
  if (err != ESP_OK) {
    gCamera.online = false;
    return false;
  }

  gCamera.consecutiveCaptureFailures = 0;
  reapplyDesiredControlsUnlocked(esp_camera_sensor_get());
  refreshStatusFromSensor();
  return gCamera.online;
}

bool recoverCameraLocked() {
  esp_camera_deinit();
  delay(30);
  gCamera.recoveryCount++;
  gCamera.lastRecoveryMs = millis();
  return initCameraLocked();
}

bool applyControlUnlocked(sensor_t *sensor, const String &name, int value) {
  if (!sensor) {
    return false;
  }
  if (name == "framesize") return isFrameSizeSupported(value) && sensor->set_framesize && sensor->set_framesize(sensor, static_cast<framesize_t>(value)) == 0;
  if (name == "quality") return sensor->set_quality && sensor->set_quality(sensor, value) == 0;
  if (name == "brightness") return sensor->set_brightness && sensor->set_brightness(sensor, value) == 0;
  if (name == "contrast") return sensor->set_contrast && sensor->set_contrast(sensor, value) == 0;
  if (name == "saturation") return sensor->set_saturation && sensor->set_saturation(sensor, value) == 0;
  if (name == "sharpness") return sensor->set_sharpness && sensor->set_sharpness(sensor, value) == 0;
  if (name == "special_effect") return sensor->set_special_effect && sensor->set_special_effect(sensor, value) == 0;
  if (name == "wb_mode") return sensor->set_wb_mode && sensor->set_wb_mode(sensor, value) == 0;
  if (name == "awb") return sensor->set_whitebal && sensor->set_whitebal(sensor, value ? 1 : 0) == 0;
  if (name == "awb_gain") return sensor->set_awb_gain && sensor->set_awb_gain(sensor, value ? 1 : 0) == 0;
  if (name == "aec") return sensor->set_exposure_ctrl && sensor->set_exposure_ctrl(sensor, value ? 1 : 0) == 0;
  if (name == "aec2") return sensor->set_aec2 && sensor->set_aec2(sensor, value ? 1 : 0) == 0;
  if (name == "ae_level") return sensor->set_ae_level && sensor->set_ae_level(sensor, value) == 0;
  if (name == "aec_value") return sensor->set_aec_value && sensor->set_aec_value(sensor, value) == 0;
  if (name == "agc") return sensor->set_gain_ctrl && sensor->set_gain_ctrl(sensor, value ? 1 : 0) == 0;
  if (name == "agc_gain") return sensor->set_agc_gain && sensor->set_agc_gain(sensor, value) == 0;
  if (name == "gainceiling") return sensor->set_gainceiling && sensor->set_gainceiling(sensor, static_cast<gainceiling_t>(value)) == 0;
  if (name == "hmirror") return sensor->set_hmirror && sensor->set_hmirror(sensor, value ? 1 : 0) == 0;
  if (name == "vflip") return sensor->set_vflip && sensor->set_vflip(sensor, value ? 1 : 0) == 0;
  if (name == "colorbar") return sensor->set_colorbar && sensor->set_colorbar(sensor, value ? 1 : 0) == 0;
  if (name == "dcw") return sensor->set_dcw && sensor->set_dcw(sensor, value ? 1 : 0) == 0;
  if (name == "bpc") return sensor->set_bpc && sensor->set_bpc(sensor, value ? 1 : 0) == 0;
  if (name == "wpc") return sensor->set_wpc && sensor->set_wpc(sensor, value ? 1 : 0) == 0;
  if (name == "raw_gma") return sensor->set_raw_gma && sensor->set_raw_gma(sensor, value ? 1 : 0) == 0;
  if (name == "lenc") return sensor->set_lenc && sensor->set_lenc(sensor, value ? 1 : 0) == 0;
  return false;
}

int findDesiredControl(const char *name) {
  if (!name || !name[0]) {
    return -1;
  }
  for (size_t i = 0; i < gDesiredControlCount; i++) {
    if (strncmp(gDesiredControls[i].name, name, sizeof(gDesiredControls[i].name)) == 0) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void rememberDesiredControl(const char *name, int value) {
  if (!name || !name[0]) {
    return;
  }
  int index = findDesiredControl(name);
  if (index < 0) {
    if (gDesiredControlCount >= kMaxDesiredControls) {
      return;
    }
    index = static_cast<int>(gDesiredControlCount++);
    snprintf(gDesiredControls[index].name, sizeof(gDesiredControls[index].name), "%s", name);
  }
  gDesiredControls[index].value = value;
}

void reapplyDesiredControlsUnlocked(sensor_t *sensor) {
  if (!sensor) {
    return;
  }
  for (size_t i = 0; i < gDesiredControlCount; i++) {
    applyControlUnlocked(sensor, String(gDesiredControls[i].name), gDesiredControls[i].value);
    delay(15);
  }
}

bool lockCamera(TickType_t timeoutTicks) {
  if (!gCameraMutex) {
    gCameraMutex = xSemaphoreCreateMutex();
  }
  return gCameraMutex && xSemaphoreTake(gCameraMutex, timeoutTicks) == pdTRUE;
}

void unlockCamera() {
  if (gCameraMutex) {
    xSemaphoreGive(gCameraMutex);
  }
}

}  // namespace

namespace boardcamera {

bool begin() {
  if (gCamera.online) {
    return true;
  }

  if (!gCameraMutex) {
    gCameraMutex = xSemaphoreCreateMutex();
  }
  if (!lockCamera(pdMS_TO_TICKS(1000))) {
    return false;
  }
  const bool ok = initCameraLocked();
  unlockCamera();
  return ok;
}

bool isReady() {
  return gCamera.online;
}

const BoardCameraStatus &status() {
  if (gCamera.online && lockCamera(pdMS_TO_TICKS(50))) {
    refreshStatusFromSensor();
    unlockCamera();
  }
  return gCamera;
}

camera_fb_t *capture() {
  if (gControlPending) {
    return nullptr;
  }
  if (!gCamera.online) {
    gCamera.captureFailures++;
    return nullptr;
  }
  if (!lockCamera(pdMS_TO_TICKS(250))) {
    gCamera.captureFailures++;
    return nullptr;
  }

  const uint32_t startedAt = millis();
  camera_fb_t *frame = esp_camera_fb_get();
  gCamera.lastCaptureDurationMs = millis() - startedAt;
  if (!frame) {
    gCamera.captureFailures++;
    gCamera.consecutiveCaptureFailures++;
    if (gCamera.consecutiveCaptureFailures >= kCaptureRecoveryFailureThreshold ||
        gCamera.lastCaptureDurationMs >= kCaptureRecoveryTimeoutMs) {
      recoverCameraLocked();
    }
    unlockCamera();
    return nullptr;
  }

  if (gCamera.lastCaptureDurationMs >= kCaptureRecoveryTimeoutMs) {
    gRecoverAfterRelease = true;
  } else {
    gCamera.consecutiveCaptureFailures = 0;
  }
  gCamera.captureCount++;
  gCamera.lastCaptureMs = millis();
  gCamera.lastFrameBytes = frame->len;
  gCamera.lastWidth = static_cast<uint16_t>(frame->width);
  gCamera.lastHeight = static_cast<uint16_t>(frame->height);
  return frame;
}

void release(camera_fb_t *frame) {
  if (frame) {
    esp_camera_fb_return(frame);
  }
  if (gRecoverAfterRelease) {
    gRecoverAfterRelease = false;
    recoverCameraLocked();
  }
  unlockCamera();
}

bool setControl(const String &name, int value) {
  gControlPending = true;
  if (!lockCamera(kCameraControlLockTimeout)) {
    gControlPending = false;
    return false;
  }
  sensor_t *sensor = esp_camera_sensor_get();
  if (!sensor) {
    recoverCameraLocked();
    sensor = esp_camera_sensor_get();
    if (!sensor) {
      unlockCamera();
      gControlPending = false;
      return false;
    }
  }

  bool ok = applyControlUnlocked(sensor, name, value);
  if (!ok) {
    recoverCameraLocked();
    sensor = esp_camera_sensor_get();
    ok = applyControlUnlocked(sensor, name, value);
  }
  if (ok) {
    rememberDesiredControl(name.c_str(), value);
  }

  refreshStatusFromSensor();
  unlockCamera();
  gControlPending = false;
  return ok;
}

bool setControls(const BoardCameraControl *controls, size_t count, size_t *appliedCount) {
  if (appliedCount) {
    *appliedCount = 0;
  }
  if (!controls || count == 0) {
    return false;
  }

  gControlPending = true;
  if (!lockCamera(kCameraControlLockTimeout)) {
    gControlPending = false;
    return false;
  }

  sensor_t *sensor = esp_camera_sensor_get();
  if (!sensor) {
    recoverCameraLocked();
    sensor = esp_camera_sensor_get();
    if (!sensor) {
      unlockCamera();
      gControlPending = false;
      return false;
    }
  }

  size_t applied = 0;
  bool ok = true;
  for (size_t i = 0; i < count; i++) {
    const BoardCameraControl &control = controls[i];
    if (!control.name || !applyControlUnlocked(sensor, String(control.name), control.value)) {
      ok = false;
      break;
    }
    rememberDesiredControl(control.name, control.value);
    applied++;
    delay(20);
  }

  refreshStatusFromSensor();
  unlockCamera();
  gControlPending = false;
  if (appliedCount) {
    *appliedCount = applied;
  }
  return ok && applied == count;
}

bool readRegister(uint16_t reg, uint8_t mask, int *value) {
  if (!lockCamera(pdMS_TO_TICKS(250))) {
    return false;
  }
  sensor_t *sensor = esp_camera_sensor_get();
  if (!sensor || !sensor->get_reg || !value) {
    unlockCamera();
    return false;
  }
  *value = sensor->get_reg(sensor, reg, mask);
  const bool ok = *value >= 0;
  unlockCamera();
  return ok;
}

bool writeRegister(uint16_t reg, uint8_t mask, uint8_t value) {
  if (!lockCamera(pdMS_TO_TICKS(250))) {
    return false;
  }
  sensor_t *sensor = esp_camera_sensor_get();
  if (!sensor || !sensor->set_reg) {
    unlockCamera();
    return false;
  }
  const bool ok = sensor->set_reg(sensor, reg, mask, value) == 0;
  unlockCamera();
  return ok;
}

const char *frameSizeName(int frameSize) {
  switch (frameSize) {
    case FRAMESIZE_QQVGA:
      return "QQVGA";
    case FRAMESIZE_QVGA:
      return "QVGA";
    case FRAMESIZE_VGA:
      return "VGA";
    case FRAMESIZE_SVGA:
      return "SVGA";
    case FRAMESIZE_XGA:
      return "XGA";
    default:
      return "OTHER";
  }
}

int frameSizeFromName(const String &name) {
  if (name == "QQVGA") return FRAMESIZE_QQVGA;
  if (name == "QVGA") return FRAMESIZE_QVGA;
  if (name == "VGA") return FRAMESIZE_VGA;
  if (name == "SVGA") return FRAMESIZE_SVGA;
  if (name == "XGA") return FRAMESIZE_XGA;
  return -1;
}

bool isSupportedFrameSize(int frameSize) {
  return isFrameSizeSupported(frameSize);
}

}  // namespace boardcamera
