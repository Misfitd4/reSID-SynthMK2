#pragma once

#include <AudioStream.h>
#include <Arduino.h>

#include "reSID16/sid.h"

// Simple reSID-backed audio stream for standalone synthesizer use.
// Provides a lightweight event queue so higher level code can enqueue
// SID register writes without emulating the full C64 bus.
class AudioStreamReSIDSynth : public AudioStream {
public:
  AudioStreamReSIDSynth();

  void init(float clockHz = 985248.0f, float sampleRate = AUDIO_SAMPLE_RATE_EXACT);
  void reset();
  void setChipModel(bool use8580);
  void writeRegister(uint8_t reg, uint8_t value, uint32_t delayCycles = 0);
  void clearQueue();

protected:
  void update() override;

private:
  struct Event {
    uint8_t reg;
    uint8_t value;
    uint32_t delay;
  };

  static constexpr uint32_t kQueueSize = 256;
  static constexpr uint32_t kQueueMask = kQueueSize - 1;

  void applyPendingEvents(int32_t cyclesElapsed);

  SID16 sid_;
  Event queue_[kQueueSize];

  uint32_t writeIndex_;
  uint32_t readIndex_;
  bool queueEmpty_;

  int32_t cyclesUntilNextEvent_;
  uint32_t cyclesPerSample_;
  float clockHz_;
  float sampleRate_;
};
