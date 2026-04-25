#include "board_camera.h"

#include "xl9555.h"

#include <freertos/semphr.h>
#include <string.h>

namespace {

BoardCameraStatus gCamera;
SemaphoreHandle_t gCameraMutex = nullptr;

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

bool applySetter(int (*setter)(sensor_t *, int), int value) {
  sensor_t *sensor = esp_camera_sensor_get();
  if (!sensor || !setter) {
    return false;
  }
  const bool ok = setter(sensor, value) == 0;
  refreshStatusFromSensor();
  return ok;
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

  gCamera.psram = psramFound();
  gCamera.externalClock = true;
  if (!gCameraMutex) {
    gCameraMutex = xSemaphoreCreateMutex();
  }
  resetModule();

  camera_config_t config = {};
  config.pin_pwdn = -1;
  config.pin_reset = -1;
  config.pin_xclk = -1;
  config.pin_sccb_sda = kPinSccbSda;
  config.pin_sccb_scl = kPinSccbScl;
  config.pin_d7 = kPinD7;
  config.pin_d6 = kPinD6;
  config.pin_d5 = kPinD5;
  config.pin_d4 = kPinD4;
  config.pin_d3 = kPinD3;
  config.pin_d2 = kPinD2;
  config.pin_d1 = kPinD1;
  config.pin_d0 = kPinD0;
  config.pin_vsync = kPinVsync;
  config.pin_href = kPinHref;
  config.pin_pclk = kPinPclk;
  config.xclk_freq_hz = 20000000;
  config.ledc_timer = LEDC_TIMER_1;
  config.ledc_channel = LEDC_CHANNEL_1;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QQVGA;
  config.jpeg_quality = 30;
  config.fb_count = 2;
  config.fb_location = gCamera.psram ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.sccb_i2c_port = 1;

  const esp_err_t err = esp_camera_init(&config);
  gCamera.initError = static_cast<int>(err);
  if (err != ESP_OK) {
    gCamera.online = false;
    return false;
  }

  refreshStatusFromSensor();
  return gCamera.online;
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
    unlockCamera();
    return nullptr;
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
  unlockCamera();
}

bool setControl(const String &name, int value) {
  if (!lockCamera(pdMS_TO_TICKS(250))) {
    return false;
  }
  sensor_t *sensor = esp_camera_sensor_get();
  if (!sensor) {
    unlockCamera();
    return false;
  }

  bool ok = false;
  if (name == "framesize") {
    ok = isSupportedFrameSize(value) && sensor->set_framesize && sensor->set_framesize(sensor, static_cast<framesize_t>(value)) == 0;
  } else if (name == "quality") {
    ok = sensor->set_quality && sensor->set_quality(sensor, constrain(value, 4, 63)) == 0;
  } else if (name == "brightness") {
    ok = applySetter(sensor->set_brightness, constrain(value, -2, 2));
  } else if (name == "contrast") {
    ok = applySetter(sensor->set_contrast, constrain(value, -2, 2));
  } else if (name == "saturation") {
    ok = applySetter(sensor->set_saturation, constrain(value, -2, 2));
  } else if (name == "sharpness") {
    ok = applySetter(sensor->set_sharpness, constrain(value, -2, 2));
  } else if (name == "special_effect") {
    ok = applySetter(sensor->set_special_effect, constrain(value, 0, 6));
  } else if (name == "wb_mode") {
    ok = applySetter(sensor->set_wb_mode, constrain(value, 0, 4));
  } else if (name == "awb") {
    ok = applySetter(sensor->set_whitebal, value ? 1 : 0);
  } else if (name == "awb_gain") {
    ok = applySetter(sensor->set_awb_gain, value ? 1 : 0);
  } else if (name == "aec") {
    ok = applySetter(sensor->set_exposure_ctrl, value ? 1 : 0);
  } else if (name == "aec2") {
    ok = applySetter(sensor->set_aec2, value ? 1 : 0);
  } else if (name == "ae_level") {
    ok = applySetter(sensor->set_ae_level, constrain(value, -2, 2));
  } else if (name == "aec_value") {
    ok = applySetter(sensor->set_aec_value, constrain(value, 0, 1200));
  } else if (name == "agc") {
    ok = applySetter(sensor->set_gain_ctrl, value ? 1 : 0);
  } else if (name == "agc_gain") {
    ok = applySetter(sensor->set_agc_gain, constrain(value, 0, 30));
  } else if (name == "gainceiling") {
    ok = sensor->set_gainceiling && sensor->set_gainceiling(sensor, static_cast<gainceiling_t>(constrain(value, 0, 6))) == 0;
  } else if (name == "hmirror") {
    ok = applySetter(sensor->set_hmirror, value ? 1 : 0);
  } else if (name == "vflip") {
    ok = applySetter(sensor->set_vflip, value ? 1 : 0);
  } else if (name == "colorbar") {
    ok = applySetter(sensor->set_colorbar, value ? 1 : 0);
  } else if (name == "dcw") {
    ok = applySetter(sensor->set_dcw, value ? 1 : 0);
  } else if (name == "bpc") {
    ok = applySetter(sensor->set_bpc, value ? 1 : 0);
  } else if (name == "wpc") {
    ok = applySetter(sensor->set_wpc, value ? 1 : 0);
  } else if (name == "raw_gma") {
    ok = applySetter(sensor->set_raw_gma, value ? 1 : 0);
  } else if (name == "lenc") {
    ok = applySetter(sensor->set_lenc, value ? 1 : 0);
  }

  refreshStatusFromSensor();
  unlockCamera();
  return ok;
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
  return frameSize == FRAMESIZE_QQVGA ||
         frameSize == FRAMESIZE_QVGA ||
         frameSize == FRAMESIZE_VGA ||
         frameSize == FRAMESIZE_SVGA ||
         frameSize == FRAMESIZE_XGA;
}

}  // namespace boardcamera
