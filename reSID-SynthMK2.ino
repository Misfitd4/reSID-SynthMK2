#include <Arduino.h>
#include <Audio.h>
#include <USBHost_t36.h>
#include <usbMIDI.h>
#include <math.h>

#include "AudioStreamReSIDSynth.h"

// Audio graph
AudioStreamReSIDSynth sidVoice;
AudioOutputI2S audioOutput;
AudioConnection patchCord1(sidVoice, 0, audioOutput, 0);
AudioConnection patchCord2(sidVoice, 1, audioOutput, 1);
AudioControlSGTL5000 audioShield;

// USB host MIDI
USBHost usbHost;
USBHub hub1(usbHost);
MIDIDevice_BigBuffer usbMidi(usbHost);

// Simple state
static int currentNote = -1;
static uint8_t voiceControlBase = 0x20;  // saw wave
static uint8_t voiceControlValue = voiceControlBase;

uint16_t midiNoteToSid(uint8_t note);
void handleNoteOn(uint8_t channel, uint8_t note, uint8_t velocity);
void handleNoteOff(uint8_t channel, uint8_t note, uint8_t velocity);
void setGate(bool enabled);

struct SerialMidiParser {
  uint8_t status = 0;
  uint8_t data[2] = {0};
  uint8_t index = 0;

  void feed(uint8_t byte) {
    if (byte & 0x80) {
      status = byte;
      index = 0;
      return;
    }
    if (!status) return;

    uint8_t msgType = status & 0xF0;
    uint8_t expected = (msgType == 0xC0 || msgType == 0xD0) ? 1 : 2;

    if (index < expected) {
      data[index++] = byte;
    }

    if (index >= expected) {
      uint8_t channel = status & 0x0F;
      if (msgType == 0x90) {
        if (data[1] == 0) {
          handleNoteOff(channel, data[0], data[1]);
        } else {
          handleNoteOn(channel, data[0], data[1]);
        }
      } else if (msgType == 0x80) {
        handleNoteOff(channel, data[0], data[1]);
      }
      index = 0;
    }
  }
};

SerialMidiParser serialMidi;

void configureVoiceDefaults() {
  sidVoice.writeRegister(0x18, 0x0F);  // max volume, no filter
  sidVoice.writeRegister(0x05, 0x24);  // attack=2, decay=4
  sidVoice.writeRegister(0x06, 0xF0);  // sustain=max, release=0
  sidVoice.writeRegister(0x02, 0x00);  // pulse width low
  sidVoice.writeRegister(0x03, 0x08);  // pulse width high (approx 50%)
  sidVoice.writeRegister(0x04, voiceControlValue);
}

uint16_t midiNoteToSid(uint8_t note) {
  float freqHz = 440.0f * powf(2.0f, (static_cast<int>(note) - 69) / 12.0f);
  float sidValue = freqHz * 16777216.0f / 985248.0f;
  if (sidValue < 0.0f) sidValue = 0.0f;
  if (sidValue > 65535.0f) sidValue = 65535.0f;
  return static_cast<uint16_t>(sidValue);
}

void setGate(bool enabled) {
  uint8_t control = voiceControlBase | (enabled ? 0x01 : 0x00);
  voiceControlValue = control;
  sidVoice.writeRegister(0x04, control);
}

void noteOffAll() {
  setGate(false);
  currentNote = -1;
}

void handleNoteOn(uint8_t /*channel*/, uint8_t note, uint8_t velocity) {
  if (velocity == 0) {
    handleNoteOff(0, note, 0);
    return;
  }

  uint16_t sidFreq = midiNoteToSid(note);
  sidVoice.writeRegister(0x00, sidFreq & 0xFF);
  sidVoice.writeRegister(0x01, sidFreq >> 8);
  setGate(true);
  currentNote = note;
}

void handleNoteOff(uint8_t /*channel*/, uint8_t note, uint8_t /*velocity*/) {
  if (currentNote == note) {
    noteOffAll();
  }
}

void processUsbDeviceMidi() {
  while (usbMIDI.read()) {
    switch (usbMIDI.getType()) {
      case usbMIDI.NoteOn:
        handleNoteOn(usbMIDI.getChannel(), usbMIDI.getData1(), usbMIDI.getData2());
        break;
      case usbMIDI.NoteOff:
        handleNoteOff(usbMIDI.getChannel(), usbMIDI.getData1(), usbMIDI.getData2());
        break;
      default:
        break;
    }
  }
}

void processUsbHostMidi() {
  while (usbMidi.read()) {
    uint8_t type = usbMidi.getType();
    if (type == usbMIDI.NoteOn) {
      handleNoteOn(usbMidi.getChannel(), usbMidi.getData1(), usbMidi.getData2());
    } else if (type == usbMIDI.NoteOff) {
      handleNoteOff(usbMidi.getChannel(), usbMidi.getData1(), usbMidi.getData2());
    }
  }
}

void processSerialMidi() {
  while (Serial8.available()) {
    uint8_t b = Serial8.read();
    serialMidi.feed(b);
  }
}

void setup() {
  AudioMemory(128);
  sidVoice.init();
  sidVoice.setChipModel(false);  // default to 6581
  configureVoiceDefaults();

  audioShield.enable();
  audioShield.inputSelect(AUDIO_INPUT_LINEIN);
  audioShield.volume(0.6f);

  usbHost.begin();
  Serial8.begin(31250);
}

void loop() {
  usbHost.Task();
  processUsbHostMidi();
  processUsbDeviceMidi();
  processSerialMidi();
}
