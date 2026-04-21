///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 SDRDemon Project                                           //
//                                                                               //
// Morse Code Encoder — plugin implementation                                    //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
///////////////////////////////////////////////////////////////////////////////////

#include <QtPlugin>
#include "plugin/pluginapi.h"

#ifndef SERVER_MODE
#include "morsecodeguiencoder.h"
#endif
#include "morsecodeencoder.h"
#include "morsecodencoderwebapiadapter.h"
#include "morsecodeplugin.h"

const PluginDescriptor MorseCodeEncoderPlugin::m_pluginDescriptor = {
    MorseCodeEncoder::m_channelId,
    QStringLiteral("Morse Code Encoder"),
    QStringLiteral("1.0.0"),
    QStringLiteral("(c) SDRDemon Project"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

MorseCodeEncoderPlugin::MorseCodeEncoderPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(nullptr)
{
}

const PluginDescriptor& MorseCodeEncoderPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void MorseCodeEncoderPlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;
    m_pluginAPI->registerTxChannel(MorseCodeEncoder::m_channelIdURI, MorseCodeEncoder::m_channelId, this);
}

void MorseCodeEncoderPlugin::createTxChannel(DeviceAPI *deviceAPI, BasebandSampleSource **bs, ChannelAPI **cs) const
{
    if (bs || cs)
    {
        MorseCodeEncoder *instance = new MorseCodeEncoder(deviceAPI);
        if (bs) { *bs = instance; }
        if (cs) { *cs = instance; }
    }
}

#ifdef SERVER_MODE
ChannelGUI* MorseCodeEncoderPlugin::createTxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSource *txChannel) const
{
    (void) deviceUISet;
    (void) txChannel;
    return nullptr;
}
#else
ChannelGUI* MorseCodeEncoderPlugin::createTxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSource *txChannel) const
{
    return MorseCodeEncoderGUI::create(m_pluginAPI, deviceUISet, txChannel);
}
#endif

ChannelWebAPIAdapter* MorseCodeEncoderPlugin::createChannelWebAPIAdapter() const
{
    return new MorseCodeEncoderWebAPIAdapter();
}
