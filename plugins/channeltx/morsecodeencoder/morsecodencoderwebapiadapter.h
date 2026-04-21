///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 SDRDemon Project                                           //
//                                                                               //
// Morse Code Encoder — WebAPI adapter header                                    //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_MORSECODENCODERWEBAPIADAPTER_H
#define INCLUDE_MORSECODENCODERWEBAPIADAPTER_H

#include "channel/channelwebapiadapter.h"
#include "morsecodencodersettings.h"

class MorseCodeEncoderWebAPIAdapter : public ChannelWebAPIAdapter {
public:
    MorseCodeEncoderWebAPIAdapter();
    virtual ~MorseCodeEncoderWebAPIAdapter();

    virtual QByteArray serialize() const { return m_settings.serialize(); }
    virtual bool deserialize(const QByteArray& data) { return m_settings.deserialize(data); }

    virtual int webapiSettingsGet(SWGSDRangel::SWGChannelSettings& response, QString& errorMessage);
    virtual int webapiSettingsPutPatch(bool force, const QStringList& channelSettingsKeys,
                                       SWGSDRangel::SWGChannelSettings& response, QString& errorMessage);

private:
    MorseCodeEncoderSettings m_settings;
};

#endif // INCLUDE_MORSECODENCODERWEBAPIADAPTER_H
