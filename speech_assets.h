#ifndef SPEECH_ASSETS_H
#define SPEECH_ASSETS_H

#include <Arduino.h>

namespace speechassets {

struct Clip {
  const int16_t *samples;
  size_t sampleCount;
  const char *label;
};

extern const Clip kPrefix;
extern const Clip kDegree;
extern const Clip kTen;
extern const Clip kZero;
extern const Clip kOne;
extern const Clip kTwo;
extern const Clip kThree;
extern const Clip kFour;
extern const Clip kFive;
extern const Clip kSix;
extern const Clip kSeven;
extern const Clip kEight;
extern const Clip kNine;

}  // namespace speechassets

#endif
