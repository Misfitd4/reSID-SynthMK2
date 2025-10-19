# reSID-SynthMK2

This is a slim reSID-based synthesizer forked from SIDKick and adapted for a
plain Teensy 4.1 with the PJRC Audio Shield.

## Features

- Monophonic MIDI-to-SID voice driven by the reSID16 emulator.
- Audio output over I2S to the SGTL5000 codec.
- MIDI input from:
  - Teensy's built-in USB device port (`usbMIDI`),
  - the USB host port (`USBHost_t36`), and
  - a DIN connection wired to `Serial8` (31250 baud).

## Folder layout

```
AudioStreamReSIDSynth.*   // Minimal AudioStream wrapper with a write queue
reSID-SynthMK2.ino        // Arduino sketch entry point
reSID16/                  // Dag Lem's reSID 0.16 fixed-point core (GPL)
LICENSE                   // GPLv3 license inherited from SIDKick
```

## Building

1. Install Teensyduino (1.59+) or set up PlatformIO with `teensy41` support.
2. Open `reSID-SynthMK2.ino` in the Arduino IDE.
3. Select `Teensy 4.1`, USB type `Serial + MIDI + Audio`, and an appropriate
   optimization setting (default works).
4. Compile & upload.

This code is still experimental—latency handling, voice management and filter
routing are intentionally simple to leave room for your own extensions.

## Roadmap ideas

- Polyphony or duophonic layering via multiple SID instances.
- MIDI CC mapping for filter cutoff/resonance and waveform selection.
- Preset storage using Teensy's flash or an SD card.
- Optional re-introduction of fmOPL for hybrid SID/OPL voices.

