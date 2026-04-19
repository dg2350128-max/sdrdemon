# CW Modulator Plugin

## Description

The CW Modulator plugin encodes plain text as Morse code (ITU CW) and transmits it as an on-off-keyed (OOK) carrier at the selected frequency offset.

It uses the existing `Morse` utility from `sdrbase` for text-to-Morse conversion, making it compatible with all standard ITU characters as well as many non-standard ones.

## Settings

| Parameter | Description |
|-----------|-------------|
| **Df** | Carrier frequency offset from the device center frequency (Hz) |
| **Speed (WPM)** | Transmission speed in words per minute (1–60 WPM). The dit duration is computed as 1200/WPM milliseconds following the PARIS standard. |
| **Gain** | Output gain in dB (−50 to 0 dB) |
| **M** (Mute) | Mutes the channel output |
| **Text** | The text to encode and transmit |
| **CW** | Read-only Morse preview of the text (• = dit, − = dah) |
| **Tx** | Start transmitting the current text |
| **Repeat** | Repeat the transmission the configured number of times (0 = infinite) |

## Timing

Morse timing follows the standard PARIS word timing:

| Element | Duration |
|---------|----------|
| Dit (•) | 1 unit |
| Dah (−) | 3 units |
| Inter-element gap | 1 unit |
| Inter-character gap | 3 units |
| Inter-word gap | 7 units |

where 1 unit = 1200 / WPM milliseconds.

## Operation

1. Enter the text you wish to transmit in the **Text** field.
2. The **CW** field shows the Morse encoding of your text.
3. Click **Tx** to begin transmission.
4. Enable **Repeat** and set a repeat count if you want to loop the transmission.

## WebAPI

Full WebAPI support is not yet available (requires swagger spec additions). The plugin serializes and deserializes its settings correctly for session file save/load.
