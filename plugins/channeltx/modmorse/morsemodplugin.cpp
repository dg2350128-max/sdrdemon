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

#include <QtPlugin>
#include "plugin/pluginapi.h"

#ifndef SERVER_MODE
#include "morsemodgui.h"
#endif
#include "morsemod.h"
#include "morsemodwebapiadapter.h"
#include "morsemodplugin.h"

const PluginDescriptor MorseModPlugin::m_pluginDescriptor = {
    MorseMod::m_channelId,
    QStringLiteral("Morse (CW) Modulator"),
    QStringLiteral("1.0.0"),
    QStringLiteral("(c) SDRangel contributors"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

MorseModPlugin::MorseModPlugin(QObject *parent) :
    QObject(parent),
    m_pluginAPI(nullptr)
{
}

const PluginDescriptor& MorseModPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void MorseModPlugin::initPlugin(PluginAPI *pluginAPI)
{
    m_pluginAPI = pluginAPI;
    m_pluginAPI->registerTxChannel(MorseMod::m_channelIdURI, MorseMod::m_channelId, this);
}

void MorseModPlugin::createTxChannel(DeviceAPI *deviceAPI, BasebandSampleSource **bs, ChannelAPI **cs) const
{
    if (bs || cs)
    {
        MorseMod *instance = new MorseMod(deviceAPI);

        if (bs) {
            *bs = instance;
        }
        if (cs) {
            *cs = instance;
        }
    }
}

#ifdef SERVER_MODE
ChannelGUI* MorseModPlugin::createTxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSource *txChannel) const
{
    (void)deviceUISet;
    (void)txChannel;
    return nullptr;
}
#else
ChannelGUI* MorseModPlugin::createTxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSource *txChannel) const
{
    return MorseModGUI::create(m_pluginAPI, deviceUISet, txChannel);
}
#endif

ChannelWebAPIAdapter* MorseModPlugin::createChannelWebAPIAdapter() const
{
    return new MorseModWebAPIAdapter();
}
