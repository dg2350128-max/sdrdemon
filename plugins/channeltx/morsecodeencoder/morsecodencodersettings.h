///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 SDRDemon Project                                           //
//                                                                               //
// Morse Code Encoder Plugin for HackRF                                          //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef PLUGINS_CHANNELTX_MORSECODEENCODER_MORSECODERENCODER_SETTINGS_H
#define PLUGINS_CHANNELTX_MORSECODEENCODER_MORSECODERENCODER_SETTINGS_H

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <stdint.h>
#include "dsp/dsptypes.h"

class Serializable;

/** Settings for the Morse Code Encoder channel plugin.
 *
 *  This plugin extends the CW modulator concept by providing:
 *  - A rich message editor with live Morse preview
 *  - A message queue / log
 *  - Prosign support (AR, BT, SK, KN …)
 *  - Configurable inter-character and inter-word spacing multipliers
 */
struct MorseCodeEncoderSettings
{
    qint64 m_inputFrequencyOffset;  //!< Channel offset from device centre frequency (Hz)
    Real m_rfBandwidth;             //!< RF bandwidth (Hz)
    Real m_toneFrequency;           //!< CW sidetone / carrier pitch (Hz)
    Real m_gain;                    //!< Gain (dB)
    bool m_channelMute;             //!< Mute the channel output
    int m_wpm;                      //!< CW speed in words-per-minute (Paris standard)
    bool m_loop;                    //!< Loop the current message
    QString m_text;                 //!< Text to encode and transmit
    QStringList m_messageQueue;     //!< Pending messages waiting to be sent
    QStringList m_predefinedTexts;  //!< Quick-access predefined messages
    bool m_useRiseTime;             //!< Apply raised-cosine key shaping
    float m_riseTime;               //!< Rise/fall time in milliseconds
    float m_charSpaceMultiplier;    //!< Multiplier for inter-character spacing (default 3)
    float m_wordSpaceMultiplier;    //!< Multiplier for inter-word spacing (default 7)
    bool m_showMorsePreview;        //!< Display dot/dash preview while typing

    quint32 m_rgbColor;
    QString m_title;
    Serializable *m_channelMarker;
    int m_streamIndex;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    MorseCodeEncoderSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const MorseCodeEncoderSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force = false) const;
};

#endif // PLUGINS_CHANNELTX_MORSECODEENCODER_MORSECODERENCODER_SETTINGS_H
