# CW Modulator plugin

## Introduction

The CW Modulator plugin generates a Continuous Wave (CW) signal keyed with Morse code.
It is specifically designed for use with the **HackRF One** and **HackRF Pro** transmit-capable SDR devices,
though it works with any SDRangel-compatible transmitter.

CW (Continuous Wave) is the oldest form of radio communication.  The carrier is switched on and off
according to the timing of Morse code symbols (dots, dashes, inter-element spaces, inter-character
spaces and inter-word spaces).

## Interface

| Control | Description |
|---------|-------------|
| **Δf** | Channel offset from the device centre frequency (Hz) |
| **RF BW** | RF bandwidth of the transmitted signal (Hz) |
| **Tone Hz** | Sidetone / carrier pitch frequency (Hz).  Typically 600–800 Hz for CW |
| **WPM** | CW speed in words per minute (Paris standard) |
| **Gain** | Output gain in dB (0 = maximum) |
| **Mute** | Silence the channel without stopping the CW keyer |
| **Loop** | Repeat the message continuously |
| **Message** | Text to encode as Morse code and transmit |
| **TX** | Trigger immediate transmission of the current message |
| **Level meter** | Real-time output power indicator |

## HackRF One/Pro notes

The HackRF One and HackRF Pro provide up to +15 dBm output power at frequencies from 1 MHz to 6 GHz.
For CW operation:

* Set the device sample rate to at least 2 MS/s.
* Set the HackRF TX VGA gain and TX LNA gain to appropriate values to avoid spectral spurii.
* Use a low-pass filter on the RF output when operating near amateur-radio band edges.
* Ensure your local regulations permit transmission on the chosen frequency.

## Morse code speed guide

| WPM | Dot length (ms) |
|-----|----------------|
| 5   | 240            |
| 10  | 120            |
| 15  | 80             |
| 20  | 60             |
| 25  | 48             |
| 30  | 40             |

## Signal chain

```
Text → MorseEncoder (CWKeyer) → OOK keying → Carrier NCO → UpChannelizer → HackRF TX
```

The rise/fall time of each element is shaped using a raised-cosine envelope (configurable
via `riseTime` in the settings) to reduce key clicks.
