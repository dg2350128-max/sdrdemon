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

#ifndef PLUGINS_CHANNELTX_MODMORSE_MORSEMOD_H
#define PLUGINS_CHANNELTX_MODMORSE_MORSEMOD_H

#include <QRecursiveMutex>
#include <QNetworkRequest>

#include "dsp/basebandsamplesource.h"
#include "dsp/spectrumvis.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "morsemodsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class QUdpSocket;
class DeviceAPI;
class MorseModBaseband;
class ObjectPipe;

/** Morse (CW) transmit channel. */
class MorseMod : public BasebandSampleSource, public ChannelAPI
{
public:
    /** Configuration message sent to the channel and baseband. */
    class MsgConfigureMorseMod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const MorseModSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingKeys() const { return m_settingKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureMorseMod* create(const QStringList& settingKeys, const MorseModSettings& settings, bool force)
        {
            return new MsgConfigureMorseMod(settingKeys, settings, force);
        }

    private:
        MorseModSettings m_settings;
        QStringList m_settingKeys;
        bool m_force;

        MsgConfigureMorseMod(const QStringList& settingKeys, const MorseModSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_settingKeys(settingKeys),
            m_force(force)
        {}
    };

    /** Trigger transmission of the stored m_text setting. */
    class MsgTx : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgTx* create() { return new MsgTx(); }

    private:
        MsgTx() : Message() {}
    };

    /** Report sent from source to GUI with the character currently being transmitted. */
    class MsgReportTx : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getText() const { return m_text; }
        int getBufferedCharacters() const { return m_bufferedCharacters; }

        static MsgReportTx* create(const QString& text, int bufferedCharacters) {
            return new MsgReportTx(text, bufferedCharacters);
        }

    private:
        QString m_text;
        int m_bufferedCharacters;

        MsgReportTx(const QString& text, int bufferedCharacters) :
            Message(),
            m_text(text),
            m_bufferedCharacters(bufferedCharacters)
        {}
    };

    /** Trigger transmission of custom text. */
    class MsgTXText : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgTXText* create(const QString& text) { return new MsgTXText(text); }
        QString m_text;

    private:
        MsgTXText(const QString& text) : Message(), m_text(text) {}
    };

    //=========================================================================

    MorseMod(DeviceAPI *deviceAPI);
    virtual ~MorseMod();
    virtual void destroy() { delete this; }
    virtual void setDeviceAPI(DeviceAPI *deviceAPI);
    virtual DeviceAPI *getDeviceAPI() { return m_deviceAPI; }

    virtual void start();
    virtual void stop();
    virtual void pull(SampleVector::iterator& begin, unsigned int nbSamples);
    virtual void pushMessage(Message *msg) { m_inputMessageQueue.push(msg); }
    virtual QString getSourceName() { return objectName(); }

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual QString getIdentifier() const { return objectName(); }
    virtual void getTitle(QString& title) { title = m_settings.m_title; }
    virtual qint64 getCenterFrequency() const { return m_settings.m_inputFrequencyOffset; }
    virtual void setCenterFrequency(qint64 frequency);

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int getNbSinkStreams() const { return 1; }
    virtual int getNbSourceStreams() const { return 0; }
    virtual int getStreamIndex() const { return m_settings.m_streamIndex; }

    virtual qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const
    {
        (void)streamIndex;
        (void)sinkElseSource;
        return m_settings.m_inputFrequencyOffset;
    }

    virtual int webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage);

    virtual int webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage);

    virtual int webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage);

    virtual int webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage);

    virtual int webapiActionsPost(
        const QStringList& channelActionsKeys,
        SWGSDRangel::SWGChannelActions& query,
        QString& errorMessage);

    /** No-op stub: Morse plugin does not populate SWG channel settings objects. */
    static void webapiFormatChannelSettings(
        SWGSDRangel::SWGChannelSettings& response,
        const MorseModSettings& settings)
    {
        (void)response;
        (void)settings;
    }

    /** No-op stub: Morse plugin does not read SWG channel settings objects. */
    static void webapiUpdateChannelSettings(
        MorseModSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
    {
        (void)settings;
        (void)channelSettingsKeys;
        (void)response;
    }

    SpectrumVis *getSpectrumVis() { return &m_spectrumVis; }
    double getMagSq() const;
    void setLevelMeter(QObject *levelMeter);
    uint32_t getNumberOfDeviceStreams() const;
    int getSourceChannelSampleRate() const;
    void setMessageQueueToGUI(MessageQueue* queue) final;

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

private:
    DeviceAPI *m_deviceAPI;
    QThread *m_thread;
    MorseModBaseband *m_basebandSource;
    MorseModSettings m_settings;
    SpectrumVis m_spectrumVis;

    SampleVector m_sampleBuffer;
    QRecursiveMutex m_settingsMutex;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;
    QUdpSocket *m_udpSocket;

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const QStringList& settingsKeys, const MorseModSettings& settings, bool force = false);
    void sendSampleRateToDemodAnalyzer();
    void webapiReverseSendSettings(const QList<QString>& channelSettingsKeys, const MorseModSettings& settings, bool force);
    void sendChannelSettings(
        const QList<ObjectPipe*>& pipes,
        const QList<QString>& channelSettingsKeys,
        const MorseModSettings& settings,
        bool force);
    void openUDP(const MorseModSettings& settings);
    void closeUDP();

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void udpRx();
};

#endif // PLUGINS_CHANNELTX_MODMORSE_MORSEMOD_H
