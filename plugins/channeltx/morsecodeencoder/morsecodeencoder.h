///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 SDRDemon Project                                           //
//                                                                               //
// Morse Code Encoder — main channel class header                                //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
///////////////////////////////////////////////////////////////////////////////////

#ifndef PLUGINS_CHANNELTX_MORSECODEENCODER_MORSECODEENCODER_H
#define PLUGINS_CHANNELTX_MORSECODEENCODER_MORSECODEENCODER_H

#include <QRecursiveMutex>
#include <QNetworkRequest>

#include "dsp/basebandsamplesource.h"
#include "dsp/spectrumvis.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "morsecodencodersettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class DeviceAPI;
class MorseCodeEncoderBaseband;
class ObjectPipe;

/** Morse Code Encoder TX channel.
 *
 *  Encodes text as Morse code and generates an OOK RF signal.
 *  Extends the CW modulator concept with a message queue, live Morse preview,
 *  and configurable spacing multipliers.
 *  Designed for HackRF One / HackRF Pro.
 */
class MorseCodeEncoder : public BasebandSampleSource, public ChannelAPI {
public:
    class MsgConfigureMorseCodeEncoder : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        const MorseCodeEncoderSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingKeys() const { return m_settingKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureMorseCodeEncoder* create(const QStringList& settingKeys, const MorseCodeEncoderSettings& settings, bool force)
        {
            return new MsgConfigureMorseCodeEncoder(settingKeys, settings, force);
        }

    private:
        MorseCodeEncoderSettings m_settings;
        QStringList m_settingKeys;
        bool m_force;

        MsgConfigureMorseCodeEncoder(const QStringList& settingKeys, const MorseCodeEncoderSettings& settings, bool force) :
            Message(), m_settings(settings), m_settingKeys(settingKeys), m_force(force) { }
    };

    class MsgTx : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        static MsgTx* create() { return new MsgTx(); }
    private:
        MsgTx() : Message() { }
    };

    class MsgTXText : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        static MsgTXText* create(const QString& text) { return new MsgTXText(text); }
        QString m_text;
    private:
        MsgTXText(const QString& text) : Message(), m_text(text) { }
    };

    //=================================================================

    MorseCodeEncoder(DeviceAPI *deviceAPI);
    virtual ~MorseCodeEncoder();
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
        (void) streamIndex; (void) sinkElseSource;
        return m_settings.m_inputFrequencyOffset;
    }

    virtual int webapiSettingsGet(SWGSDRangel::SWGChannelSettings& response, QString& errorMessage);
    virtual int webapiWorkspaceGet(SWGSDRangel::SWGWorkspaceInfo& response, QString& errorMessage);
    virtual int webapiSettingsPutPatch(bool force, const QStringList& channelSettingsKeys,
                                       SWGSDRangel::SWGChannelSettings& response, QString& errorMessage);
    virtual int webapiActionsPost(const QStringList& channelActionsKeys,
                                  SWGSDRangel::SWGChannelActions& query, QString& errorMessage);

    SpectrumVis *getSpectrumVis() { return &m_spectrumVis; }
    double getMagSq() const;
    void setLevelMeter(QObject *levelMeter);
    uint32_t getNumberOfDeviceStreams() const;
    int getSourceChannelSampleRate() const;
    void setMessageQueueToGUI(MessageQueue* queue) final;

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

private:
    DeviceAPI* m_deviceAPI;
    QThread *m_thread;
    MorseCodeEncoderBaseband* m_basebandSource;
    MorseCodeEncoderSettings m_settings;
    SpectrumVis m_spectrumVis;
    SampleVector m_sampleBuffer;
    QRecursiveMutex m_settingsMutex;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const QStringList& settingsKeys, const MorseCodeEncoderSettings& settings, bool force = false);
    void sendSampleRateToDemodAnalyzer();
    void webapiReverseSendSettings(const QList<QString>& keys, const MorseCodeEncoderSettings& settings, bool force);
    void sendChannelSettings(const QList<ObjectPipe*>& pipes, const QList<QString>& keys,
                              const MorseCodeEncoderSettings& settings, bool force);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // PLUGINS_CHANNELTX_MORSECODEENCODER_MORSECODEENCODER_H
