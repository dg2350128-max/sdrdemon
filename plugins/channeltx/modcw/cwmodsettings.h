///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 SDRDemon contributors                                     //
//                                                                               //
// Morse Encoder (CW) modulator plugin — settings                               //
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

#ifndef PLUGINS_CHANNELTX_MODCW_CWMODSETTINGS_H
#define PLUGINS_CHANNELTX_MODCW_CWMODSETTINGS_H

#include <QByteArray>
#include <QString>
#include <stdint.h>
#include "dsp/dsptypes.h"

class Serializable;

/** Settings for the Morse Encoder (CW) transmit channel plugin. */
struct CWModSettings
{
    // RF / channel
    qint64 m_inputFrequencyOffset;  ///< Offset from the device centre frequency (Hz)
    int    m_rfBandwidth;           ///< Low-pass filter bandwidth (Hz)
    Real   m_gain;                  ///< RF gain (dB)
    bool   m_channelMute;           ///< Silence the channel without removing it

    // Morse timing
    float  m_wpm;                   ///< Morse speed (words per minute, PARIS standard)

    // Text source
    QString m_text;                 ///< Text to convert and transmit

    // Repeat
    bool   m_repeat;                ///< Repeat transmission
    int    m_repeatCount;           ///< Number of times to repeat (-1 = infinite)

    // USB CW Keyer
    bool    m_keyerEnabled;         ///< Enable USB serial keyer
    QString m_keyerPort;            ///< Serial port name for the USB keyer (e.g. COM3, /dev/ttyUSB0)

    // Cosmetic / serialisation bookkeeping
    quint32 m_rgbColor;
    QString m_title;
    Serializable *m_channelMarker;
    int    m_streamIndex;
    bool   m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    Serializable *m_rollupState;
    int    m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool   m_hidden;

    CWModSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState)     { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const CWModSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force = false) const;
};

#endif // PLUGINS_CHANNELTX_MODCW_CWMODSETTINGS_H
