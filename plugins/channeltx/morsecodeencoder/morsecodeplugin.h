///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 SDRDemon Project                                           //
//                                                                               //
// Morse Code Encoder — plugin header                                            //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_MORSECODEPLUGIN_H
#define INCLUDE_MORSECODEPLUGIN_H

#include <QObject>
#include "plugin/plugininterface.h"

class DeviceUISet;
class BasebandSampleSource;

class MorseCodeEncoderPlugin : public QObject, PluginInterface {
    Q_OBJECT
    Q_INTERFACES(PluginInterface)
    Q_PLUGIN_METADATA(IID "sdrangel.channeltx.morsecodeencoder")

public:
    explicit MorseCodeEncoderPlugin(QObject* parent = nullptr);

    const PluginDescriptor& getPluginDescriptor() const;
    void initPlugin(PluginAPI* pluginAPI);

    virtual void createTxChannel(DeviceAPI *deviceAPI, BasebandSampleSource **bs, ChannelAPI **cs) const;
    virtual ChannelGUI* createTxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSource *txChannel) const;
    virtual ChannelWebAPIAdapter* createChannelWebAPIAdapter() const;

private:
    static const PluginDescriptor m_pluginDescriptor;
    PluginAPI* m_pluginAPI;
};

#endif // INCLUDE_MORSECODEPLUGIN_H
