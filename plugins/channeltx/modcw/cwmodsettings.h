///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 SDRDemon Project                                           //
//                                                                               //
// CW (Continuous Wave) Modulator Plugin for HackRF One/Pro                      //
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
#include <QStringList>
#include <stdint.h>
#include "dsp/dsptypes.h"
#include "dsp/cwkeyersettings.h"

class Serializable;

/** Settings for the CW (Continuous Wave) Modulator channel plugin. */
struct CWModSettings
{
    qint64 m_inputFrequencyOffset;  //!< Offset from device centre frequency (Hz)
    Real m_rfBandwidth;             //!< RF bandwidth (Hz)
    Real m_toneFrequency;           //!< CW sidetone / carrier pitch (Hz)
    Real m_gain;                    //!< Gain (dB)
    bool m_channelMute;             //!< Mute the channel
    int m_wpm;                      //!< CW speed in words-per-minute
    bool m_loop;                    //!< Loop the message
    QString m_text;                 //!< Text to transmit as Morse code
    QStringList m_predefinedTexts;  //!< Predefined CQ / QSO texts
    bool m_useRiseTime;             //!< Apply raised-cosine key shaping
    float m_riseTime;               //!< Rise/fall time in milliseconds

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

    CWModSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const CWModSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force = false) const;
};

#endif // PLUGINS_CHANNELTX_MODCW_CWMODSETTINGS_H
