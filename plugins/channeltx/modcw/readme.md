# Morse Encoder (CW Modulator)

<small>Belongs to the **SDRangel** transmit channel plugins family.</small>

## Description

The **Morse Encoder** plugin converts plain text to International Morse Code (ITU) and transmits it using **On-Off Keying (OOK)** of a carrier. The carrier is keyed on for dots and dashes, and off for the inter-element, inter-character and inter-word gaps.

It also supports **USB CW Keyers** that present as a serial (COM / ttyUSB) port, allowing real-time keying of the transmitter from a physical paddle or straight key.

## Features

- **Text-to-Morse**: enter any text; it is converted automatically using the ITU Morse table (letters, digits, punctuation).
- **Adjustable speed**: 5–60 WPM (words per minute) following the PARIS standard (50 units per word).
- **OOK modulation**: clean on/off keying of the configured carrier frequency offset.
- **Repeat**: optionally repeat the transmission a configurable number of times.
- **USB CW Keyer**: detect and open any plug-and-play USB serial keyer. The plugin monitors the CTS handshake line and also accepts raw bytes from the serial port (any non-zero byte = key down, 0x00 = key up).
- **Spectrum view**: built-in spectrum analyser.
- **Standard channel controls**: frequency offset, RF bandwidth, output gain, channel mute.

## Interface

| Control | Description |
|---------|-------------|
| **Δf** | Frequency offset from device centre frequency (Hz) |
| **RF BW** | Low-pass filter bandwidth (Hz) |
| **Gain** | Output gain (dB) |
| **WPM** | Morse speed in words per minute |
| **Mute** | Silence the channel without removing it |
| **Text** | Text to convert and transmit |
| **TX** | Start transmitting the current text |
| **Repeat / Count** | Repeat the text *n* times |
| **Clear** | Clear the transmitted-text log |
| **Enable keyer** | Open a USB serial CW keyer |
| **Port** | Serial port name (auto-populated from OS) |
| **↺** | Refresh the serial port list |

## Morse Timing

The following timing is used (PARIS standard):

| Symbol | Duration |
|--------|----------|
| Dot    | 1 unit on |
| Dash   | 3 units on |
| Inter-element gap | 1 unit off |
| Inter-character gap | 3 units off total |
| Inter-word gap | 7 units off total |

At speed *W* WPM: **1 unit = 1200 / W milliseconds**.

## USB CW Keyer Support

Many inexpensive CW paddles and straight keys connect via a USB-to-serial chip (e.g. CH340, CP2102, FTDI). The plugin opens the selected COM/ttyUSB port and:

1. Monitors the **CTS (Clear To Send) line**: key down → CTS asserted → carrier on.
2. Accepts **serial bytes**: non-zero byte received → carrier on; zero byte → carrier off.

Some commercial keyers (e.g. WinKeyer-compatible devices) may additionally send speed or sidetone information; those bytes are silently ignored beyond the key-state detection above.

> **Note:** The plugin uses `Qt::SerialPort` which requires the `Qt5/6SerialPort` package. Build flags enable this automatically (`Qt::SerialPort` is added to `target_link_libraries` in `CMakeLists.txt`).

## API

Currently the WebAPI adapter returns HTTP 200 for GET/PUT-PATCH settings requests without populating a body. Full Swagger integration is planned for a future release.
