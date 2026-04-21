///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 SDRangel contributors                                      //
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

#ifndef PLUGINS_CHANNELTX_MODMORSE_MORSEMODSETTINGS_H
#define PLUGINS_CHANNELTX_MODMORSE_MORSEMODSETTINGS_H

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <stdint.h>
#include "dsp/dsptypes.h"

class Serializable;

/** Settings for the Morse (CW) modulator channel plugin. */
struct MorseModSettings
{
    qint64 m_inputFrequencyOffset;   ///< Channel frequency offset in Hz
    int m_toneFrequency;             ///< CW sidetone frequency in Hz
    Real m_gain;                     ///< Output gain in dB
    bool m_channelMute;              ///< Mute the channel output
    int m_wpm;                       ///< Speed in words per minute
    bool m_repeat;                   ///< Repeat transmission
    int m_repeatCount;               ///< Number of repeats (when m_repeat is true)
    QString m_text;                  ///< Text to transmit
    QStringList m_predefinedTexts;   ///< List of preset messages

    quint32 m_rgbColor;
    QString m_title;
    Serializable *m_channelMarker;
    int m_streamIndex;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    bool m_udpEnabled;
    QString m_udpAddress;
    uint16_t m_udpPort;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    MorseModSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const MorseModSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force = false) const;
};

#endif // PLUGINS_CHANNELTX_MODMORSE_MORSEMODSETTINGS_H
