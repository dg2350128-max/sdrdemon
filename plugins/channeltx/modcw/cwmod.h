///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 SDRDemon contributors                                     //
//                                                                               //
// Morse Encoder (CW) modulator plugin — main channel class                     //
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

#ifndef PLUGINS_CHANNELTX_MODCW_CWMOD_H_
#define PLUGINS_CHANNELTX_MODCW_CWMOD_H_

#include <QRecursiveMutex>
#include <QNetworkRequest>

#include "dsp/basebandsamplesource.h"
#include "dsp/spectrumvis.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "cwmodsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class DeviceAPI;
class CWModBaseband;
class ObjectPipe;

/** Main channel class for the Morse Encoder (CW) transmit plugin. */
class CWMod : public BasebandSampleSource, public ChannelAPI {
public:
    // -----------------------------------------------------------------------
    // Messages
    // -----------------------------------------------------------------------

    class MsgConfigureCWMod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const CWModSettings& getSettings() const   { return m_settings; }
        const QStringList& getSettingKeys() const  { return m_settingKeys; }
        bool getForce() const                      { return m_force; }

        static MsgConfigureCWMod* create(
            const QStringList& settingKeys,
            const CWModSettings& settings,
            bool force)
        {
            return new MsgConfigureCWMod(settingKeys, settings, force);
        }

    private:
        CWModSettings  m_settings;
        QStringList    m_settingKeys;
        bool           m_force;

        MsgConfigureCWMod(
            const QStringList& settingKeys,
            const CWModSettings& settings,
            bool force) :
            Message(),
            m_settings(settings),
            m_settingKeys(settingKeys),
            m_force(force)
        {}
    };

    /** Trigger transmission of m_settings.m_text. */
    class MsgTx : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgTx* create() { return new MsgTx(); }

    private:
        MsgTx() : Message() {}
    };

    /** Progress report sent from the source thread to the GUI. */
    class MsgReportTx : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getText() const          { return m_text; }
        int getBufferedCharacters() const        { return m_bufferedCharacters; }

        static MsgReportTx* create(const QString& text, int bufferedCharacters) {
            return new MsgReportTx(text, bufferedCharacters);
        }

    private:
        QString m_text;
        int     m_bufferedCharacters;

        MsgReportTx(const QString& text, int bufferedCharacters) :
            Message(), m_text(text), m_bufferedCharacters(bufferedCharacters)
        {}
    };

    /** Send an arbitrary text string (e.g. from the GUI text box). */
    class MsgTXText : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgTXText* create(const QString& text) {
            return new MsgTXText(text);
        }

        QString m_text;

    private:
        MsgTXText(const QString& text) : Message(), m_text(text) {}
    };

    // -----------------------------------------------------------------------
    // ChannelAPI / BasebandSampleSource interface
    // -----------------------------------------------------------------------

    CWMod(DeviceAPI* deviceAPI);
    ~CWMod() override;

    void destroy() override { delete this; }
    void setDeviceAPI(DeviceAPI* deviceAPI) override;
    DeviceAPI* getDeviceAPI() override { return m_deviceAPI; }

    void start() override;
    void stop() override;
    void pull(SampleVector::iterator& begin, unsigned int nbSamples) override;
    void pushMessage(Message* msg) override { m_inputMessageQueue.push(msg); }
    QString getSourceName() override { return objectName(); }

    void getIdentifier(QString& id) override  { id = objectName(); }
    QString getIdentifier() const override    { return objectName(); }
    void getTitle(QString& title) override    { title = m_settings.m_title; }
    qint64 getCenterFrequency() const override { return m_settings.m_inputFrequencyOffset; }
    void setCenterFrequency(qint64 frequency) override;

    QByteArray serialize() const override;
    bool deserialize(const QByteArray& data) override;

    int getNbSinkStreams() const override    { return 1; }
    int getNbSourceStreams() const override  { return 0; }
    int getStreamIndex() const override     { return m_settings.m_streamIndex; }

    qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const override
    {
        (void)streamIndex; (void)sinkElseSource;
        return m_settings.m_inputFrequencyOffset;
    }

    virtual int webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage) override;

    virtual int webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage) override;

    virtual int webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage) override;

    virtual int webapiActionsPost(
        const QStringList& channelActionsKeys,
        SWGSDRangel::SWGChannelActions& query,
        QString& errorMessage) override;

    SpectrumVis* getSpectrumVis()       { return &m_spectrumVis; }
    double getMagSq() const;
    void setLevelMeter(QObject* levelMeter);
    uint32_t getNumberOfDeviceStreams() const;
    int getSourceChannelSampleRate() const;
    void setMessageQueueToGUI(MessageQueue* queue) final;

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

private:
    DeviceAPI*       m_deviceAPI;
    QThread*         m_thread;
    CWModBaseband*   m_basebandSource;
    CWModSettings    m_settings;
    SpectrumVis      m_spectrumVis;

    QNetworkAccessManager* m_networkManager;
    QNetworkRequest        m_networkRequest;

    bool handleMessage(const Message& cmd) override;
    void applySettings(const QStringList& settingsKeys, const CWModSettings& settings, bool force = false);
    void sendSampleRateToDemodAnalyzer();
    void webapiReverseSendSettings(
        const QList<QString>& channelSettingsKeys,
        const CWModSettings& settings,
        bool force);
    void sendChannelSettings(
        const QList<ObjectPipe*>& pipes,
        const QList<QString>& channelSettingsKeys,
        const CWModSettings& settings,
        bool force);

private slots:
    void networkManagerFinished(QNetworkReply* reply);
};

#endif // PLUGINS_CHANNELTX_MODCW_CWMOD_H_
