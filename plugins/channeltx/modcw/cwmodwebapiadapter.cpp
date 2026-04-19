///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 SDRDemon contributors                                     //
//                                                                               //
// Morse Encoder (CW) modulator plugin — WebAPI adapter implementation          //
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

#include "SWGChannelSettings.h"
#include "cwmodwebapiadapter.h"

int CWModWebAPIAdapter::webapiSettingsGet(
    SWGSDRangel::SWGChannelSettings& response,
    QString& errorMessage)
{
    (void)response;
    (void)errorMessage;
    // Full Swagger body population would go here
    return 200;
}

int CWModWebAPIAdapter::webapiSettingsPutPatch(
    bool force,
    const QStringList& channelSettingsKeys,
    SWGSDRangel::SWGChannelSettings& response,
    QString& errorMessage)
{
    (void)force;
    (void)channelSettingsKeys;
    (void)response;
    (void)errorMessage;
    return 200;
}
