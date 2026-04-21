///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 SDRDemon Project                                           //
//                                                                               //
// Morse Code Encoder — main channel implementation                              //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
///////////////////////////////////////////////////////////////////////////////////

#include <QTime>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QThread>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGChannelActions.h"

#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "util/db.h"
#include "maincore.h"

#include "morsecodencoderbaseband.h"
#include "morsecodeencoder.h"

MESSAGE_CLASS_DEFINITION(MorseCodeEncoder::MsgConfigureMorseCodeEncoder, Message)
MESSAGE_CLASS_DEFINITION(MorseCodeEncoder::MsgTx, Message)
MESSAGE_CLASS_DEFINITION(MorseCodeEncoder::MsgTXText, Message)

const char* const MorseCodeEncoder::m_channelIdURI = "sdrangel.channeltx.morsecodeencoder";
const char* const MorseCodeEncoder::m_channelId = "MorseCodeEncoder";

MorseCodeEncoder::MorseCodeEncoder(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSource),
    m_deviceAPI(deviceAPI),
    m_spectrumVis(SDR_TX_SCALEF)
{
    setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSource = new MorseCodeEncoderBaseband();
    m_basebandSource->setSpectrumSampleSink(&m_spectrumVis);
    m_basebandSource->setChannel(this);
    m_basebandSource->moveToThread(m_thread);

    applySettings(QStringList(), m_settings, true);

    m_deviceAPI->addChannelSource(this);
    m_deviceAPI->addChannelSourceAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &MorseCodeEncoder::networkManagerFinished
    );
}

MorseCodeEncoder::~MorseCodeEncoder()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &MorseCodeEncoder::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSourceAPI(this);
    m_deviceAPI->removeChannelSource(this, true);
    stop();
    delete m_basebandSource;
    delete m_thread;
}

void MorseCodeEncoder::setDeviceAPI(DeviceAPI *deviceAPI)
{
    if (deviceAPI != m_deviceAPI)
    {
        m_deviceAPI->removeChannelSourceAPI(this);
        m_deviceAPI->removeChannelSource(this, false);
        m_deviceAPI = deviceAPI;
        m_deviceAPI->addChannelSource(this);
        m_deviceAPI->addChannelSinkAPI(this);
    }
}

void MorseCodeEncoder::start()
{
    qDebug("MorseCodeEncoder::start");
    m_basebandSource->reset();
    m_thread->start();
}

void MorseCodeEncoder::stop()
{
    qDebug("MorseCodeEncoder::stop");
    m_thread->exit();
    m_thread->wait();
}

void MorseCodeEncoder::pull(SampleVector::iterator& begin, unsigned int nbSamples)
{
    m_basebandSource->pull(begin, nbSamples);
}

bool MorseCodeEncoder::handleMessage(const Message& cmd)
{
    if (MsgConfigureMorseCodeEncoder::match(cmd))
    {
        MsgConfigureMorseCodeEncoder& cfg = (MsgConfigureMorseCodeEncoder&) cmd;
        qDebug() << "MorseCodeEncoder::handleMessage: MsgConfigureMorseCodeEncoder";
        applySettings(cfg.getSettingKeys(), cfg.getSettings(), cfg.getForce());
        return true;
    }
    else if (MsgTx::match(cmd))
    {
        MsgTx* msg = new MsgTx((const MsgTx&)cmd);
        m_basebandSource->getInputMessageQueue()->push(msg);
        return true;
    }
    else if (MsgTXText::match(cmd))
    {
        MsgTXText* msg = new MsgTXText((const MsgTXText&)cmd);
        m_basebandSource->getInputMessageQueue()->push(msg);
        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        DSPSignalNotification* rep = new DSPSignalNotification(notif);
        qDebug() << "MorseCodeEncoder::handleMessage: DSPSignalNotification";
        m_basebandSource->getInputMessageQueue()->push(rep);

        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new DSPSignalNotification(notif));
        }
        return true;
    }
    else if (MainCore::MsgChannelDemodQuery::match(cmd))
    {
        qDebug() << "MorseCodeEncoder::handleMessage: MsgChannelDemodQuery";
        sendSampleRateToDemodAnalyzer();
        return true;
    }
    else
    {
        return false;
    }
}

void MorseCodeEncoder::setCenterFrequency(qint64 frequency)
{
    MorseCodeEncoderSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(QStringList("inputFrequencyOffset"), settings, false);

    if (m_guiMessageQueue)
    {
        MsgConfigureMorseCodeEncoder *msgToGUI = MsgConfigureMorseCodeEncoder::create(
            QStringList("inputFrequencyOffset"), settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void MorseCodeEncoder::applySettings(const QStringList& settingsKeys, const MorseCodeEncoderSettings& settings, bool force)
{
    qDebug() << "MorseCodeEncoder::applySettings:" << settings.getDebugString(settingsKeys, force);

    if (settingsKeys.contains("streamIndex") && m_settings.m_streamIndex != settings.m_streamIndex)
    {
        if (m_deviceAPI->getSampleMIMO())
        {
            m_deviceAPI->removeChannelSourceAPI(this);
            m_deviceAPI->removeChannelSource(this, false, m_settings.m_streamIndex);
            m_deviceAPI->addChannelSource(this, settings.m_streamIndex);
            m_deviceAPI->addChannelSourceAPI(this);
            m_settings.m_streamIndex = settings.m_streamIndex;
            emit streamIndexChanged(settings.m_streamIndex);
        }
    }

    MorseCodeEncoderBaseband::MsgConfigureMorseCodeEncoderBaseband *msg =
        MorseCodeEncoderBaseband::MsgConfigureMorseCodeEncoderBaseband::create(settingsKeys, settings, force);
    m_basebandSource->getInputMessageQueue()->push(msg);

    if (settingsKeys.contains("useReverseAPI") && settings.m_useReverseAPI) {
        webapiReverseSendSettings(settingsKeys, settings, force);
    }

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "settings", pipes);

    if (pipes.size() > 0) {
        sendChannelSettings(pipes, settingsKeys, settings, force);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

QByteArray MorseCodeEncoder::serialize() const
{
    return m_settings.serialize();
}

bool MorseCodeEncoder::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureMorseCodeEncoder *msg = MsgConfigureMorseCodeEncoder::create(QStringList(), m_settings, true);
    m_inputMessageQueue.push(msg);

    return success;
}

void MorseCodeEncoder::sendSampleRateToDemodAnalyzer()
{
    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "reportdemod", pipes);

    for (const auto& pipe : pipes)
    {
        MessageQueue* messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        MainCore::MsgChannelDemodReport *msg = MainCore::MsgChannelDemodReport::create(this, getSourceChannelSampleRate());
        messageQueue->push(msg);
    }
}

int MorseCodeEncoder::webapiSettingsGet(SWGSDRangel::SWGChannelSettings& response, QString& errorMessage)
{
    (void) response;
    errorMessage = "MorseCodeEncoder WebAPI not yet fully implemented";
    return 501;
}

int MorseCodeEncoder::webapiWorkspaceGet(SWGSDRangel::SWGWorkspaceInfo& response, QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int MorseCodeEncoder::webapiSettingsPutPatch(bool force, const QStringList& channelSettingsKeys,
                                              SWGSDRangel::SWGChannelSettings& response, QString& errorMessage)
{
    (void) force; (void) channelSettingsKeys; (void) response;
    errorMessage = "MorseCodeEncoder WebAPI not yet fully implemented";
    return 501;
}

int MorseCodeEncoder::webapiActionsPost(const QStringList& channelActionsKeys,
                                         SWGSDRangel::SWGChannelActions& query, QString& errorMessage)
{
    (void) channelActionsKeys; (void) query;
    errorMessage = "MorseCodeEncoder WebAPI actions not yet fully implemented";
    return 501;
}

double MorseCodeEncoder::getMagSq() const
{
    return m_basebandSource->getMagSq();
}

void MorseCodeEncoder::setLevelMeter(QObject *levelMeter)
{
    connect(m_basebandSource, SIGNAL(levelChanged(qreal, qreal, int)), levelMeter, SLOT(levelChanged(qreal, qreal, int)));
}

uint32_t MorseCodeEncoder::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSinkStreams();
}

int MorseCodeEncoder::getSourceChannelSampleRate() const
{
    return m_basebandSource->getSourceChannelSampleRate();
}

void MorseCodeEncoder::setMessageQueueToGUI(MessageQueue* queue)
{
    ChannelAPI::setMessageQueueToGUI(queue);
    m_basebandSource->setMessageQueueToGUI(queue);
}

void MorseCodeEncoder::webapiReverseSendSettings(const QList<QString>& keys, const MorseCodeEncoderSettings& settings, bool force)
{
    (void) keys; (void) settings; (void) force;
}

void MorseCodeEncoder::sendChannelSettings(const QList<ObjectPipe*>& pipes, const QList<QString>& keys,
                                            const MorseCodeEncoderSettings& settings, bool force)
{
    (void) pipes; (void) keys; (void) settings; (void) force;
}

void MorseCodeEncoder::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();
    if (replyError) {
        qWarning() << "MorseCodeEncoder::networkManagerFinished: error(" << (int)replyError << "): " << reply->errorString();
    }
    reply->deleteLater();
}
