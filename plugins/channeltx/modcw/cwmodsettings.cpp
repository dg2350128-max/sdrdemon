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

#include <QColor>
#include <QDebug>

#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "cwmodsettings.h"

CWModSettings::CWModSettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void CWModSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 3000.0f;
    m_toneFrequency = 600.0f;
    m_gain = 0.0f;
    m_channelMute = false;
    m_wpm = 15;
    m_loop = false;
    m_text = "CQ CQ CQ DE SDRangel";
    m_predefinedTexts = QStringList({
        "CQ CQ CQ DE ${callsign} ${callsign} CQ",
        "DE ${callsign} ${callsign} ${callsign}",
        "UR 599 QTH IS ${location}",
        "TU DE ${callsign} CQ",
        "VVV VVV VVV DE SDRangel TEST"
    });
    m_useRiseTime = true;
    m_riseTime = 5.0f;
    m_rgbColor = QColor(255, 160, 32).rgb();
    m_title = "CW Modulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray CWModSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS64(1, m_inputFrequencyOffset);
    s.writeReal(2, m_rfBandwidth);
    s.writeReal(3, m_toneFrequency);
    s.writeReal(4, m_gain);
    s.writeBool(5, m_channelMute);
    s.writeS32(6, m_wpm);
    s.writeBool(7, m_loop);
    s.writeString(8, m_text);
    s.writeBool(9, m_useRiseTime);
    s.writeFloat(10, m_riseTime);
    s.writeU32(11, m_rgbColor);
    s.writeString(12, m_title);
    s.writeS32(13, m_streamIndex);
    s.writeBool(14, m_useReverseAPI);
    s.writeString(15, m_reverseAPIAddress);
    s.writeU32(16, m_reverseAPIPort);
    s.writeU32(17, m_reverseAPIDeviceIndex);
    s.writeU32(18, m_reverseAPIChannelIndex);
    s.writeS32(19, m_workspaceIndex);
    s.writeBlob(20, m_geometryBytes);
    s.writeBool(21, m_hidden);

    for (int i = 0; i < m_predefinedTexts.size(); i++) {
        s.writeString(30 + i, m_predefinedTexts[i]);
    }

    if (m_channelMarker) {
        s.writeBlob(25, m_channelMarker->serialize());
    }
    if (m_rollupState) {
        s.writeBlob(26, m_rollupState->serialize());
    }

    return s.final();
}

bool CWModSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid()) {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        QByteArray bytetmp;
        uint32_t utmp;
        QString strtmp;

        d.readS64(1, &m_inputFrequencyOffset, 0);
        d.readReal(2, &m_rfBandwidth, 3000.0f);
        d.readReal(3, &m_toneFrequency, 600.0f);
        d.readReal(4, &m_gain, 0.0f);
        d.readBool(5, &m_channelMute, false);
        d.readS32(6, &m_wpm, 15);
        d.readBool(7, &m_loop, false);
        d.readString(8, &m_text, "CQ CQ CQ DE SDRangel");
        d.readBool(9, &m_useRiseTime, true);
        d.readFloat(10, &m_riseTime, 5.0f);
        d.readU32(11, &m_rgbColor, QColor(255, 160, 32).rgb());
        d.readString(12, &m_title, "CW Modulator");
        d.readS32(13, &m_streamIndex, 0);
        d.readBool(14, &m_useReverseAPI, false);
        d.readString(15, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(16, &utmp, 0);
        m_reverseAPIPort = utmp & 0xFFFF;
        d.readU32(17, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp & 0xFFFF;
        d.readU32(18, &utmp, 0);
        m_reverseAPIChannelIndex = utmp & 0xFFFF;
        d.readS32(19, &m_workspaceIndex, 0);
        d.readBlob(20, &m_geometryBytes);
        d.readBool(21, &m_hidden, false);

        m_predefinedTexts.clear();
        for (int i = 0; i < 10; i++)
        {
            if (d.readString(30 + i, &strtmp, "")) {
                m_predefinedTexts.append(strtmp);
            }
        }

        if (m_channelMarker) {
            d.readBlob(25, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }
        if (m_rollupState) {
            d.readBlob(26, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void CWModSettings::applySettings(const QStringList& settingsKeys, const CWModSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth")) {
        m_rfBandwidth = settings.m_rfBandwidth;
    }
    if (settingsKeys.contains("toneFrequency")) {
        m_toneFrequency = settings.m_toneFrequency;
    }
    if (settingsKeys.contains("gain")) {
        m_gain = settings.m_gain;
    }
    if (settingsKeys.contains("channelMute")) {
        m_channelMute = settings.m_channelMute;
    }
    if (settingsKeys.contains("wpm")) {
        m_wpm = settings.m_wpm;
    }
    if (settingsKeys.contains("loop")) {
        m_loop = settings.m_loop;
    }
    if (settingsKeys.contains("text")) {
        m_text = settings.m_text;
    }
    if (settingsKeys.contains("useRiseTime")) {
        m_useRiseTime = settings.m_useRiseTime;
    }
    if (settingsKeys.contains("riseTime")) {
        m_riseTime = settings.m_riseTime;
    }
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
    }
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("streamIndex")) {
        m_streamIndex = settings.m_streamIndex;
    }
    if (settingsKeys.contains("useReverseAPI")) {
        m_useReverseAPI = settings.m_useReverseAPI;
    }
    if (settingsKeys.contains("reverseAPIAddress")) {
        m_reverseAPIAddress = settings.m_reverseAPIAddress;
    }
    if (settingsKeys.contains("reverseAPIPort")) {
        m_reverseAPIPort = settings.m_reverseAPIPort;
    }
    if (settingsKeys.contains("reverseAPIDeviceIndex")) {
        m_reverseAPIDeviceIndex = settings.m_reverseAPIDeviceIndex;
    }
    if (settingsKeys.contains("reverseAPIChannelIndex")) {
        m_reverseAPIChannelIndex = settings.m_reverseAPIChannelIndex;
    }
    if (settingsKeys.contains("workspaceIndex")) {
        m_workspaceIndex = settings.m_workspaceIndex;
    }
    if (settingsKeys.contains("geometryBytes")) {
        m_geometryBytes = settings.m_geometryBytes;
    }
    if (settingsKeys.contains("hidden")) {
        m_hidden = settings.m_hidden;
    }
    if (settingsKeys.contains("predefinedTexts")) {
        m_predefinedTexts = settings.m_predefinedTexts;
    }
}

QString CWModSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (force || settingsKeys.contains("inputFrequencyOffset")) {
        ostr << " m_inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (force || settingsKeys.contains("rfBandwidth")) {
        ostr << " m_rfBandwidth: " << m_rfBandwidth;
    }
    if (force || settingsKeys.contains("toneFrequency")) {
        ostr << " m_toneFrequency: " << m_toneFrequency;
    }
    if (force || settingsKeys.contains("gain")) {
        ostr << " m_gain: " << m_gain;
    }
    if (force || settingsKeys.contains("wpm")) {
        ostr << " m_wpm: " << m_wpm;
    }
    if (force || settingsKeys.contains("text")) {
        ostr << " m_text: " << m_text.toStdString();
    }

    return QString(ostr.str().c_str());
}
