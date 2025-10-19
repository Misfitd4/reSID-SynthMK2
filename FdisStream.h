#pragma once

#include <Arduino.h>

class AudioStreamReSIDSynth;

namespace FdisStream {

// Initialize the decoder with a target audio stream to receive SID writes.
void begin(AudioStreamReSIDSynth* target);

// Feed any pending bytes from the provided stream (normally Serial).
void poll(Stream& io);

// Reset decoder state and drop buffered timing.
void reset();

// Optional: inspect statistics counters.
struct Stats {
  uint32_t framesDecoded = 0;
  uint32_t framesDropped = 0;
  uint32_t writesForwarded = 0;
  uint32_t framesWithUnsupportedChip = 0;
};

Stats getStats();

}  // namespace FdisStream

