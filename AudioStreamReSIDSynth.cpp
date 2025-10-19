#include "AudioStreamReSIDSynth.h"
#include "reSID16/siddefs.h"

AudioStreamReSIDSynth::AudioStreamReSIDSynth()
    : AudioStream(0, nullptr),
      writeIndex_(0),
      readIndex_(0),
      queueEmpty_(true),
      cyclesUntilNextEvent_(0),
      cyclesPerSample_(0),
      clockHz_(985248.0f),
      sampleRate_(AUDIO_SAMPLE_RATE_EXACT) {}

void AudioStreamReSIDSynth::init(float clockHz, float sampleRate) {
  clockHz_ = clockHz;
  sampleRate_ = sampleRate;
  cyclesPerSample_ = static_cast<uint32_t>((clockHz_ + sampleRate_ / 2.0f) / sampleRate_);

  sid_.reset();
  sid_.set_chip_model(MOS6581);
  sid_.input(0);
  sid_.set_sampling_parameters(static_cast<double>(clockHz_),
                               SAMPLE_INTERPOLATE,
                               static_cast<double>(sampleRate_));

  clearQueue();
}

void AudioStreamReSIDSynth::reset() {
  sid_.reset();
  clearQueue();
}

void AudioStreamReSIDSynth::setChipModel(bool use8580) {
  sid_.set_chip_model(use8580 ? MOS8580 : MOS6581);
}

void AudioStreamReSIDSynth::writeRegister(uint8_t reg, uint8_t value, uint32_t delayCycles) {
  uint32_t next = (writeIndex_ + 1) & kQueueMask;
  if (!queueEmpty_ && next == readIndex_) {
    return;  // queue full, drop event
  }

  queue_[writeIndex_].reg = reg & 0x1f;
  queue_[writeIndex_].value = value;
  queue_[writeIndex_].delay = delayCycles;
  writeIndex_ = next;

  if (queueEmpty_) {
    queueEmpty_ = false;
    cyclesUntilNextEvent_ = static_cast<int32_t>(delayCycles);
  }
}

void AudioStreamReSIDSynth::clearQueue() {
  writeIndex_ = readIndex_ = 0;
  queueEmpty_ = true;
  cyclesUntilNextEvent_ = 0;
}

void AudioStreamReSIDSynth::applyPendingEvents(int32_t cyclesElapsed) {
  if (queueEmpty_) {
    return;
  }

  cyclesUntilNextEvent_ -= cyclesElapsed;

  while (!queueEmpty_ && cyclesUntilNextEvent_ <= 0) {
    const Event &ev = queue_[readIndex_];
    sid_.write(ev.reg, ev.value);

    readIndex_ = (readIndex_ + 1) & kQueueMask;
    if (readIndex_ == writeIndex_) {
      queueEmpty_ = true;
      cyclesUntilNextEvent_ = 0;
    } else {
      cyclesUntilNextEvent_ += static_cast<int32_t>(queue_[readIndex_].delay);
    }
  }
}

void AudioStreamReSIDSynth::update() {
  audio_block_t *blockL = allocate();
  audio_block_t *blockR = allocate();
  if (!blockL || !blockR) {
    if (blockL) release(blockL);
    if (blockR) release(blockR);
    return;
  }

  int16_t *left = blockL->data;
  int16_t *right = blockR->data;

  for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
    applyPendingEvents(static_cast<int32_t>(cyclesPerSample_));
    sid_.clock(cyclesPerSample_);
    int32_t sample = sid_.output();

    left[i] = static_cast<int16_t>(sample);
    right[i] = static_cast<int16_t>(sample);
  }

  transmit(blockL, 0);
  transmit(blockR, 1);
  release(blockL);
  release(blockR);
}
