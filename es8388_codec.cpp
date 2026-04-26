#include "es8388_codec.h"

#include <Wire.h>

#include <driver/i2s.h>
#include <math.h>
#include "xl9555.h"

namespace {
constexpr uint8_t kAddress = 0x10;
constexpr i2s_port_t kI2sPort = I2S_NUM_0;
constexpr gpio_num_t kI2sBckPin = GPIO_NUM_46;
constexpr gpio_num_t kI2sWsPin = GPIO_NUM_9;
constexpr gpio_num_t kI2sDoPin = GPIO_NUM_10;
constexpr gpio_num_t kI2sDiPin = GPIO_NUM_14;
constexpr gpio_num_t kI2sMclkPin = GPIO_NUM_3;

bool gReady = false;
bool gDriverInstalled = false;
uint32_t gSampleRate = 16000;

void setSpeakerEnabled(bool enabled) {
  xl9555_pin_set(SPK_EN, enabled ? IO_SET_LOW : IO_SET_HIGH);
}

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

void addaCfg(bool dacEnabled, bool adcEnabled) {
  uint8_t value = 0;
  value |= (!dacEnabled) << 0;
  value |= (!adcEnabled) << 1;
  value |= (!dacEnabled) << 2;
  value |= (!adcEnabled) << 3;
  writeReg(0x02, value);
}

void inputCfg(uint8_t input) {
  writeReg(0x0A, static_cast<uint8_t>((5 * input) << 4));
}

void micGain(uint8_t gain) {
  gain &= 0x0F;
  gain |= gain << 4;
  writeReg(0x09, gain);
}

void alcCtrl(uint8_t sel, uint8_t maxgain, uint8_t mingain) {
  uint8_t value = static_cast<uint8_t>((sel << 6) | ((maxgain & 0x07) << 3) | (mingain & 0x07));
  writeReg(0x12, value);
}

void outputCfg(bool out1Enabled, bool out2Enabled) {
  uint8_t value = 0;
  value |= out1Enabled ? static_cast<uint8_t>(3U << 4) : 0;
  value |= out2Enabled ? static_cast<uint8_t>(3U << 2) : 0;
  writeReg(0x04, value);
}

void speakerVolume(uint8_t volume) {
  if (volume > 33) {
    volume = 33;
  }
  writeReg(0x30, volume);
  writeReg(0x31, volume);
}

void headsetVolume(uint8_t volume) {
  if (volume > 33) {
    volume = 33;
  }
  writeReg(0x2E, volume);
  writeReg(0x2F, volume);
}

bool analyzeMicBuffer(const int16_t *buffer, size_t sampleCount, MicLevelReading *reading) {
  const size_t monoSampleCount = sampleCount / 2;
  if (monoSampleCount == 0) {
    reading->online = false;
    return false;
  }

  double sumSquares = 0.0;
  int32_t peak = 0;
  for (size_t i = 0; i < sampleCount; i += 2) {
    const int32_t sample = buffer[i];
    sumSquares += static_cast<double>(sample) * static_cast<double>(sample);
    const int32_t absSample = abs(sample);
    if (absSample > peak) {
      peak = absSample;
    }
  }

  const double rmsCounts = sqrt(sumSquares / static_cast<double>(monoSampleCount));
  const double rms = rmsCounts / 32768.0;
  reading->rms = static_cast<float>(rms);
  reading->dbfs = static_cast<float>(20.0 * log10(rms > 1e-6 ? rms : 1e-6));
  reading->peak = static_cast<int16_t>(peak);
  reading->online = true;
  return true;
}

void fillToneBuffer(int16_t *buffer, size_t frameCount, uint16_t frequencyHz, float amplitude, size_t *phaseCursor) {
  for (size_t i = 0; i < frameCount; i++) {
    const float phase = (2.0f * static_cast<float>(M_PI) * static_cast<float>(*phaseCursor) * static_cast<float>(frequencyHz)) /
                        static_cast<float>(gSampleRate);
    const int16_t sample = static_cast<int16_t>(sinf(phase) * amplitude * 32767.0f);
    buffer[i * 2] = sample;
    buffer[i * 2 + 1] = sample;
    (*phaseCursor)++;
  }
}

bool writeMonoSamples(const int16_t *samples, size_t sampleCount) {
  static int16_t txBuffer[512];
  const size_t frameCapacity = sizeof(txBuffer) / sizeof(txBuffer[0]) / 2;
  size_t offset = 0;

  while (offset < sampleCount) {
    const size_t chunkFrames = min<size_t>(frameCapacity, sampleCount - offset);
    for (size_t i = 0; i < chunkFrames; i++) {
      const int16_t sample = samples[offset + i];
      txBuffer[i * 2] = sample;
      txBuffer[i * 2 + 1] = sample;
    }

    size_t bytesWritten = 0;
    if (i2s_write(kI2sPort, txBuffer, chunkFrames * 2 * sizeof(int16_t), &bytesWritten, pdMS_TO_TICKS(180)) != ESP_OK) {
      return false;
    }
    offset += chunkFrames;
  }

  return true;
}

bool writeSilenceFrames(size_t frameCount) {
  static int16_t silenceBuffer[512] = {};
  const size_t frameCapacity = sizeof(silenceBuffer) / sizeof(silenceBuffer[0]) / 2;
  size_t remaining = frameCount;

  while (remaining > 0) {
    const size_t chunkFrames = min<size_t>(frameCapacity, remaining);
    size_t bytesWritten = 0;
    if (i2s_write(kI2sPort, silenceBuffer, chunkFrames * 2 * sizeof(int16_t), &bytesWritten, pdMS_TO_TICKS(180)) != ESP_OK) {
      return false;
    }
    remaining -= chunkFrames;
  }

  return true;
}

bool measureTonePhase(bool speakerEnabled,
                      uint16_t frequencyHz,
                      uint16_t durationMs,
                      uint8_t volume,
                      MicLevelReading *reading) {
  static int16_t txBuffer[320];
  static int16_t rxBuffer[320];
  if (!reading) {
    return false;
  }

  reading->online = false;
  reading->dbfs = -120.0f;
  reading->peak = 0;

  speakerVolume(volume);
  i2s_zero_dma_buffer(kI2sPort);
  setSpeakerEnabled(speakerEnabled);
  delay(18);

  size_t phaseCursor = 0;
  const size_t frameCount = sizeof(txBuffer) / sizeof(txBuffer[0]) / 2;
  const uint32_t totalFrames = (static_cast<uint32_t>(durationMs) * gSampleRate) / 1000U;
  uint32_t processedFrames = 0;

  while (processedFrames < totalFrames) {
    const size_t chunkFrames = min<size_t>(frameCount, totalFrames - processedFrames);
    fillToneBuffer(txBuffer, chunkFrames, frequencyHz, 0.26f, &phaseCursor);

    size_t bytesWritten = 0;
    if (i2s_write(kI2sPort, txBuffer, chunkFrames * 2 * sizeof(int16_t), &bytesWritten, pdMS_TO_TICKS(180)) != ESP_OK) {
      setSpeakerEnabled(false);
      return false;
    }

    size_t bytesRead = 0;
    if (i2s_read(kI2sPort, rxBuffer, chunkFrames * 2 * sizeof(int16_t), &bytesRead, pdMS_TO_TICKS(180)) == ESP_OK && bytesRead > 0) {
      MicLevelReading observed;
      if (analyzeMicBuffer(rxBuffer, bytesRead / sizeof(int16_t), &observed)) {
        reading->online = true;
        if (observed.dbfs > reading->dbfs) {
          reading->dbfs = observed.dbfs;
        }
        if (observed.peak > reading->peak) {
          reading->peak = observed.peak;
        }
      }
    }

    processedFrames += static_cast<uint32_t>(chunkFrames);
  }

  writeSilenceFrames(gSampleRate / 40U);
  setSpeakerEnabled(false);
  delay(10);
  return reading->online;
}
}  // namespace

namespace es8388codec {

bool beginMic() {
  uint8_t id = 0;
  if (!readReg(0x00, &id)) {
    gReady = false;
    return false;
  }

  writeReg(0x00, 0x80);
  writeReg(0x00, 0x00);
  delay(100);

  writeReg(0x01, 0x58);
  writeReg(0x01, 0x50);
  writeReg(0x02, 0xF3);
  writeReg(0x02, 0xF0);
  writeReg(0x03, 0x09);
  writeReg(0x00, 0x06);
  writeReg(0x04, 0x00);
  writeReg(0x08, 0x00);
  writeReg(0x2B, 0x80);
  writeReg(0x09, 0x88);
  writeReg(0x0C, 0x4C);
  writeReg(0x0D, 0x02);
  writeReg(0x10, 0x00);
  writeReg(0x11, 0x00);
  writeReg(0x17, 0x18);
  writeReg(0x18, 0x02);
  writeReg(0x1A, 0x00);
  writeReg(0x1B, 0x00);
  writeReg(0x27, 0xB8);
  writeReg(0x2A, 0xB8);
  delay(50);

  addaCfg(true, true);
  outputCfg(true, true);
  inputCfg(0);
  micGain(8);
  alcCtrl(0, 7, 0);
  headsetVolume(18);
  speakerVolume(26);
  xl9555_io_config(SPK_EN, IO_SET_OUTPUT);
  setSpeakerEnabled(false);

  if (gDriverInstalled) {
    i2s_driver_uninstall(kI2sPort);
    gDriverInstalled = false;
  }

  i2s_config_t config = {};
  config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX);
  config.sample_rate = gSampleRate;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags = 0;
  config.dma_buf_count = 8;
  config.dma_buf_len = 256;
  config.use_apll = true;
  config.tx_desc_auto_clear = true;
  config.fixed_mclk = 0;

  const esp_err_t installResult = i2s_driver_install(kI2sPort, &config, 0, nullptr);
  if (installResult != ESP_OK) {
    gReady = false;
    return false;
  }
  gDriverInstalled = true;

  i2s_pin_config_t pins = {};
  pins.bck_io_num = kI2sBckPin;
  pins.ws_io_num = kI2sWsPin;
  pins.data_out_num = kI2sDoPin;
  pins.data_in_num = kI2sDiPin;
  pins.mck_io_num = kI2sMclkPin;

  if (i2s_set_pin(kI2sPort, &pins) != ESP_OK) {
    endMic();
    gReady = false;
    return false;
  }

  i2s_zero_dma_buffer(kI2sPort);
  gReady = true;
  return true;
}

bool readMicLevel(MicLevelReading *reading) {
  static int16_t buffer[1024];
  size_t bytesRead = 0;

  if (!gReady) {
    reading->online = false;
    return false;
  }

  if (i2s_read(kI2sPort, buffer, sizeof(buffer), &bytesRead, pdMS_TO_TICKS(120)) != ESP_OK || bytesRead == 0) {
    reading->online = false;
    return false;
  }

  return analyzeMicBuffer(buffer, bytesRead / sizeof(int16_t), reading);
}

bool playMonoPcm(const int16_t *samples, size_t sampleCount, uint8_t volume) {
  if (!gReady || !samples || sampleCount == 0) {
    return false;
  }

  speakerVolume(volume);
  i2s_zero_dma_buffer(kI2sPort);
  setSpeakerEnabled(true);
  delay(12);

  const bool ok = writeMonoSamples(samples, sampleCount) && writeSilenceFrames(gSampleRate / 24U);
  setSpeakerEnabled(false);
  delay(8);
  return ok;
}

bool playTone(uint16_t frequencyHz, uint16_t durationMs, uint8_t volume) {
  static int16_t buffer[512];
  if (!gReady || frequencyHz == 0 || durationMs == 0) {
    return false;
  }

  speakerVolume(volume);
  i2s_zero_dma_buffer(kI2sPort);
  setSpeakerEnabled(true);
  delay(12);

  size_t phaseCursor = 0;
  const size_t frameCount = sizeof(buffer) / sizeof(buffer[0]) / 2;
  const uint32_t totalFrames = (static_cast<uint32_t>(durationMs) * gSampleRate) / 1000U;
  uint32_t writtenFrames = 0;
  const float amplitude = 0.22f;

  while (writtenFrames < totalFrames) {
    const size_t chunkFrames = min<size_t>(frameCount, totalFrames - writtenFrames);
    fillToneBuffer(buffer, chunkFrames, frequencyHz, amplitude, &phaseCursor);
    size_t bytesWritten = 0;
    if (i2s_write(kI2sPort, buffer, chunkFrames * 2 * sizeof(int16_t), &bytesWritten, pdMS_TO_TICKS(120)) != ESP_OK) {
      setSpeakerEnabled(false);
      return false;
    }
    writtenFrames += static_cast<uint32_t>(chunkFrames);
  }
  const bool ok = writeSilenceFrames(gSampleRate / 24U);
  setSpeakerEnabled(false);
  delay(8);
  return ok;
}

bool playToneAndMeasure(uint16_t frequencyHz, uint16_t durationMs, SpeakerLoopbackReading *reading, uint8_t volume) {
  if (!reading) {
    return false;
  }

  reading->online = false;
  reading->detected = false;
  reading->mutedDbfs = -120.0f;
  reading->enabledDbfs = -120.0f;
  reading->deltaDbfs = 0.0f;
  reading->mutedPeak = 0;
  reading->enabledPeak = 0;
  if (!gReady || frequencyHz == 0 || durationMs == 0) {
    return false;
  }

  MicLevelReading muted;
  MicLevelReading enabled;
  const bool mutedOk = measureTonePhase(false, frequencyHz, durationMs, volume, &muted);
  const bool enabledOk = measureTonePhase(true, frequencyHz, durationMs, volume, &enabled);

  if (mutedOk) {
    reading->mutedDbfs = muted.dbfs;
    reading->mutedPeak = muted.peak;
  }
  if (enabledOk) {
    reading->enabledDbfs = enabled.dbfs;
    reading->enabledPeak = enabled.peak;
  }

  reading->online = mutedOk || enabledOk;
  if (!reading->online) {
    return false;
  }

  reading->deltaDbfs = reading->enabledDbfs - reading->mutedDbfs;
  reading->detected = mutedOk &&
                      enabledOk &&
                      reading->enabledDbfs > (reading->mutedDbfs + 5.5f) &&
                      reading->enabledPeak > (reading->mutedPeak + 180);
  return true;
}

void endMic() {
  if (gDriverInstalled) {
    i2s_driver_uninstall(kI2sPort);
    gDriverInstalled = false;
  }
  gReady = false;
}

bool isReady() {
  return gReady;
}

bool readRegister(uint8_t reg, uint8_t *value) {
  return readReg(reg, value);
}

bool writeRegister(uint8_t reg, uint8_t value) {
  return writeReg(reg, value);
}

bool setMicGain(uint8_t gain) {
  if (gain > 8) {
    return false;
  }
  micGain(gain);
  return true;
}

bool setInputChannel(uint8_t input) {
  if (input > 1) {
    return false;
  }
  inputCfg(input);
  return true;
}

bool setAlcPreset(uint8_t preset) {
  switch (preset) {
    case 0:
      alcCtrl(0, 7, 0);
      return true;
    case 1:
      alcCtrl(3, 5, 1);
      writeReg(0x13, 0x70);
      writeReg(0x14, 0x32);
      writeReg(0x15, 0x06);
      return true;
    case 2:
      alcCtrl(3, 7, 0);
      writeReg(0x13, 0x90);
      writeReg(0x14, 0x21);
      writeReg(0x15, 0x86);
      return true;
    default:
      return false;
  }
}

bool set3dDepth(uint8_t depth) {
  if (depth > 7) {
    return false;
  }
  return writeReg(0x1D, static_cast<uint8_t>(depth << 2));
}

bool setAdcDacSampleRate(uint32_t sampleRate) {
  if (!(sampleRate == 8000 || sampleRate == 16000 || sampleRate == 22050 || sampleRate == 32000)) {
    return false;
  }
  if (gDriverInstalled && i2s_set_clk(kI2sPort, sampleRate, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO) != ESP_OK) {
    return false;
  }
  gSampleRate = sampleRate;
  return true;
}

uint32_t currentSampleRate() {
  return gSampleRate;
}

bool setEqPreset(uint8_t preset) {
  static const uint8_t flat[] = {0x1F, 0xF7, 0xFD, 0xFF, 0x1F, 0xF7, 0xFD, 0xFF};
  static const uint8_t voice[] = {0x17, 0xF1, 0xF8, 0xFF, 0x26, 0xF9, 0xFD, 0xFF};
  const uint8_t *profile = nullptr;
  switch (preset) {
    case 0:
      profile = flat;
      break;
    case 1:
      profile = voice;
      break;
    default:
      return false;
  }
  for (uint8_t i = 0; i < 8; i++) {
    if (!writeReg(static_cast<uint8_t>(0x1E + i), profile[i])) {
      return false;
    }
  }
  return true;
}

}  // namespace es8388codec
