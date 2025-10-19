#include "FdisStream.h"

#include <cstring>

#include "AudioStreamReSIDSynth.h"

namespace FdisStream {

// 'FDIS' magic in little endian.
static constexpr uint32_t kStreamMagic = 0x53494446u;
static constexpr size_t kHeaderSize = 12;
static constexpr size_t kEventSize = 8;

struct SidTapHeader {
  uint32_t magic;
  uint32_t count;
  uint32_t frame;
};

struct SidTapEvent {
  uint8_t chip;
  uint8_t addr;
  uint8_t value;
  uint8_t pad;
  uint32_t dt;
};

enum class Stage : uint8_t {
  Header,
  Events
};

struct DecoderState {
  Stage stage = Stage::Header;

  uint8_t headerBuf[kHeaderSize];
  size_t headerBytes = 0;
  uint32_t expectedEvents = 0;
  uint32_t frameNumber = 0;

  uint8_t eventBuf[kEventSize];
  size_t eventBytes = 0;
  uint32_t eventsRead = 0;

  bool skippingFrame = false;
  bool warnedChip = false;
};

static DecoderState decoder;
static AudioStreamReSIDSynth* targetStream = nullptr;
static Stats stats;

static inline uint32_t readLe32(const uint8_t* data) {
  return (static_cast<uint32_t>(data[0])) |
         (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) |
         (static_cast<uint32_t>(data[3]) << 24);
}

static void handleEvent(uint32_t dtCycles, uint8_t addr, uint8_t value) {
  if (!targetStream) {
    return;
  }
  targetStream->writeRegister(addr, value, dtCycles);
  stats.writesForwarded++;
}

static void finishFrame() {
  decoder.stage = Stage::Header;
  decoder.headerBytes = 0;
  decoder.eventBytes = 0;
  decoder.eventsRead = 0;
  decoder.skippingFrame = false;
  decoder.warnedChip = false;
}

static void pushHeader() {
  decoder.expectedEvents = readLe32(decoder.headerBuf + 4);
  decoder.frameNumber = readLe32(decoder.headerBuf + 8);
  decoder.eventsRead = 0;
  decoder.eventBytes = 0;
  decoder.stage = Stage::Events;
  decoder.skippingFrame = false;
  decoder.warnedChip = false;
  stats.framesDecoded++;

  if (decoder.expectedEvents == 0) {
    decoder.skippingFrame = true;
  }
}

static void processHeaderByte(uint8_t byte) {
  decoder.headerBuf[decoder.headerBytes++] = byte;

  if (decoder.headerBytes < kHeaderSize) {
    return;
  }

  uint32_t magic = readLe32(decoder.headerBuf);
  if (magic != kStreamMagic) {
    // Shift by one and continue searching for the magic preamble.
    memmove(decoder.headerBuf, decoder.headerBuf + 1, kHeaderSize - 1);
      decoder.headerBytes = kHeaderSize - 1;
      return;
  }

  pushHeader();
}

static void processEventByte(uint8_t byte) {
  decoder.eventBuf[decoder.eventBytes++] = byte;
  if (decoder.eventBytes < kEventSize) {
    return;
  }

  if (decoder.eventsRead >= decoder.expectedEvents) {
    // Safety: shouldn't happen, but reset to header stage.
    finishFrame();
    return;
  }

  uint8_t chip = decoder.eventBuf[0];
  uint8_t addr = decoder.eventBuf[1];
  uint8_t value = decoder.eventBuf[2];
  uint32_t dt = readLe32(decoder.eventBuf + 4);

  if (!decoder.skippingFrame) {
    if (chip != 0) {
      decoder.skippingFrame = true;
      stats.framesWithUnsupportedChip++;
    } else {
      handleEvent(dt, addr, value);
    }
  }

  decoder.eventsRead++;
  decoder.eventBytes = 0;

  if (decoder.eventsRead >= decoder.expectedEvents) {
    finishFrame();
  }
}

static void feedByte(uint8_t byte) {
  if (decoder.stage == Stage::Header) {
    processHeaderByte(byte);
  } else {
    processEventByte(byte);
  }
}

void begin(AudioStreamReSIDSynth* target) {
  targetStream = target;
  reset();
}

void poll(Stream& io) {
  if (!targetStream) {
    return;
  }
  while (io.available() > 0) {
    uint8_t byte = static_cast<uint8_t>(io.read());
    feedByte(byte);
  }
}

void reset() {
  decoder = DecoderState{};
}

Stats getStats() {
  return stats;
}

}  // namespace FdisStream
