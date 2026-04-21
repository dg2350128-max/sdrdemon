///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 SDRDemon Project                                           //
//                                                                               //
// Morse Code Encoder — WebAPI adapter implementation                            //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
///////////////////////////////////////////////////////////////////////////////////

#include "morsecodencoderwebapiadapter.h"

MorseCodeEncoderWebAPIAdapter::MorseCodeEncoderWebAPIAdapter() { }
MorseCodeEncoderWebAPIAdapter::~MorseCodeEncoderWebAPIAdapter() { }

int MorseCodeEncoderWebAPIAdapter::webapiSettingsGet(SWGSDRangel::SWGChannelSettings& response, QString& errorMessage)
{
    (void) response;
    errorMessage = "MorseCodeEncoder WebAPI not yet fully implemented";
    return 501;
}

int MorseCodeEncoderWebAPIAdapter::webapiSettingsPutPatch(bool force, const QStringList& channelSettingsKeys,
                                                           SWGSDRangel::SWGChannelSettings& response, QString& errorMessage)
{
    (void) force; (void) channelSettingsKeys; (void) response;
    errorMessage = "MorseCodeEncoder WebAPI not yet fully implemented";
    return 501;
}
