#ifndef ES8388_CODEC_H
#define ES8388_CODEC_H

#include <Arduino.h>

struct MicLevelReading {
  bool online = false;
  float rms = 0.0f;
  float dbfs = -120.0f;
  int16_t peak = 0;
};

struct SpeakerLoopbackReading {
  bool online = false;
  bool detected = false;
  float mutedDbfs = -120.0f;
  float enabledDbfs = -120.0f;
  float deltaDbfs = 0.0f;
  int16_t mutedPeak = 0;
  int16_t enabledPeak = 0;
};

namespace es8388codec {
bool beginMic();
bool readMicLevel(MicLevelReading *reading);
bool playMonoPcm(const int16_t *samples, size_t sampleCount, uint8_t volume = 26);
bool playTone(uint16_t frequencyHz, uint16_t durationMs, uint8_t volume = 26);
bool playToneAndMeasure(uint16_t frequencyHz, uint16_t durationMs, SpeakerLoopbackReading *reading, uint8_t volume = 26);
void endMic();
bool isReady();
}

#endif
