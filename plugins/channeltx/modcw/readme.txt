CW Modulator Plugin for SDRangel
=================================

Overview
--------
The CW Modulator (modcw) plugin generates a Morse code (CW) signal for
transmission via SDRangel. It encodes text as ITU-R M.1677-1 International
Morse Code and keys a continuous-wave carrier on and off accordingly.

A smooth raised-cosine envelope is applied during key-up and key-down
transitions to prevent audible "key clicks" in the transmitted signal.

Features
--------
- Configurable CW speed in words per minute (WPM, 1–60 WPM)
- Configurable tone frequency (carrier offset, default 700 Hz)
- Configurable RF bandwidth
- Frequency offset from device centre frequency
- Gain control (dB)
- Channel mute
- Message repeat (finite or infinite loop)
- Supports A-Z, 0-9 in standard ITU-R Morse code

Settings
--------
| Parameter              | Description                                      | Default            |
|------------------------|--------------------------------------------------|--------------------|
| Frequency offset       | Offset from device centre frequency (Hz)         | 0                  |
| RF bandwidth           | RF bandwidth of the transmitted signal (Hz)      | 500                |
| WPM                    | Morse code speed in words per minute             | 20                 |
| Tone frequency         | CW tone / carrier offset (Hz)                    | 700                |
| Gain                   | Output gain (dB)                                 | 0.0                |
| Channel mute           | Mute the channel output                          | Off                |
| Repeat                 | Loop the message after transmission              | Off                |
| Repeat count           | Number of repetitions (-1 = infinite)            | -1                 |
| Text                   | Message to encode as Morse code                  | CQ CQ DE SDRangel K |

Usage
-----
1. Add the CW Modulator channel to a transmit device in SDRangel.
2. Set the desired frequency offset, tone frequency, and WPM.
3. Enter the text to transmit in the "Text" field.
4. Click Transmit (or use the WebAPI) to start keying.

Morse Code Reference (ITU-R M.1677-1)
--------------------------------------
A .-    B -...  C -.-.  D -..   E .     F ..-.
G --.   H ....  I ..    J .---  K -.-   L .-..
M --    N -.    O ---   P .--.  Q --.-  R .-.
S ...   T -     U ..-   V ...-  W .--   X -..-
Y -.--  Z --..

0 -----  1 .----  2 ..---  3 ...--  4 ....-
5 .....  6 -....  7 --...  8 ---..  9 ----.

Build
-----
The plugin is built as part of the SDRangel project using CMake:

    cmake --preset default
    cmake --build --preset default

The compiled DLL/shared library is installed alongside other channel plugins.

Tags
----
SDRangel, Plugin, dg2350128-maxsTag
