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

#include <QTime>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkDatagram>
#include <QNetworkReply>
#include <QBuffer>
#include <QUdpSocket>
#include <QThread>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGChannelReport.h"
#include "SWGChannelActions.h"

#include <stdio.h>
#include <complex.h>
#include <algorithm>

#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "util/db.h"
#include "maincore.h"

#include "morsemodbaseband.h"
#include "morsemod.h"

MESSAGE_CLASS_DEFINITION(MorseMod::MsgConfigureMorseMod, Message)
MESSAGE_CLASS_DEFINITION(MorseMod::MsgTx, Message)
MESSAGE_CLASS_DEFINITION(MorseMod::MsgReportTx, Message)
MESSAGE_CLASS_DEFINITION(MorseMod::MsgTXText, Message)

const char* const MorseMod::m_channelIdURI = "sdrangel.channeltx.modmorse";
const char* const MorseMod::m_channelId    = "MorseMod";

MorseMod::MorseMod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSource),
    m_deviceAPI(deviceAPI),
    m_spectrumVis(SDR_TX_SCALEF),
    m_udpSocket(nullptr)
{
    setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSource = new MorseModBaseband();
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
        &MorseMod::networkManagerFinished
    );
}

MorseMod::~MorseMod()
{
    closeUDP();
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &MorseMod::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSourceAPI(this);
    m_deviceAPI->removeChannelSource(this, true);
    stop();
    delete m_basebandSource;
    delete m_thread;
}

void MorseMod::setDeviceAPI(DeviceAPI *deviceAPI)
{
    if (deviceAPI != m_deviceAPI)
    {
        m_deviceAPI->removeChannelSourceAPI(this);
        m_deviceAPI->removeChannelSource(this, false);
        m_deviceAPI = deviceAPI;
        m_deviceAPI->addChannelSource(this);
        m_deviceAPI->addChannelSourceAPI(this);
    }
}

void MorseMod::start()
{
    qDebug("MorseMod::start");
    m_basebandSource->reset();
    m_thread->start();
}

void MorseMod::stop()
{
    qDebug("MorseMod::stop");
    m_thread->exit();
    m_thread->wait();
}

void MorseMod::pull(SampleVector::iterator& begin, unsigned int nbSamples)
{
    m_basebandSource->pull(begin, nbSamples);
}

bool MorseMod::handleMessage(const Message& cmd)
{
    if (MsgConfigureMorseMod::match(cmd))
    {
        MsgConfigureMorseMod& cfg = (MsgConfigureMorseMod&)cmd;
        qDebug() << "MorseMod::handleMessage: MsgConfigureMorseMod";
        applySettings(cfg.getSettingKeys(), cfg.getSettings(), cfg.getForce());
        return true;
    }
    else if (MsgTx::match(cmd))
    {
        MsgTx *msg = new MsgTx((const MsgTx&)cmd);
        m_basebandSource->getInputMessageQueue()->push(msg);
        return true;
    }
    else if (MsgTXText::match(cmd))
    {
        MsgTXText *msg = new MsgTXText((const MsgTXText&)cmd);
        m_basebandSource->getInputMessageQueue()->push(msg);
        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&)cmd;
        DSPSignalNotification *rep = new DSPSignalNotification(notif);
        qDebug() << "MorseMod::handleMessage: DSPSignalNotification";
        m_basebandSource->getInputMessageQueue()->push(rep);
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new DSPSignalNotification(notif));
        }
        return true;
    }
    else if (MainCore::MsgChannelDemodQuery::match(cmd))
    {
        qDebug() << "MorseMod::handleMessage: MsgChannelDemodQuery";
        sendSampleRateToDemodAnalyzer();
        return true;
    }
    else
    {
        return false;
    }
}

void MorseMod::setCenterFrequency(qint64 frequency)
{
    MorseModSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(QStringList("inputFrequencyOffset"), settings, false);

    if (m_guiMessageQueue)
    {
        MsgConfigureMorseMod *msgToGUI = MsgConfigureMorseMod::create(QStringList("inputFrequencyOffset"), settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void MorseMod::applySettings(const QStringList& settingsKeys, const MorseModSettings& settings, bool force)
{
    qDebug() << "MorseMod::applySettings:" << settings.getDebugString(settingsKeys, force);

    if ((settingsKeys.contains("udpEnabled") && (settings.m_udpEnabled != m_settings.m_udpEnabled))
        || (settingsKeys.contains("udpAddress") && (settings.m_udpAddress != m_settings.m_udpAddress))
        || (settingsKeys.contains("udpPort") && (settings.m_udpPort != m_settings.m_udpPort))
        || force)
    {
        if (settings.m_udpEnabled) {
            openUDP(settings);
        } else {
            closeUDP();
        }
    }

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

    MorseModBaseband::MsgConfigureMorseModBaseband *msg =
        MorseModBaseband::MsgConfigureMorseModBaseband::create(settingsKeys, settings, force);
    m_basebandSource->getInputMessageQueue()->push(msg);

    if (settingsKeys.contains("useReverseAPI") && settings.m_useReverseAPI)
    {
        bool fullUpdate =
            ((settingsKeys.contains("useReverseAPI") && (m_settings.m_useReverseAPI != settings.m_useReverseAPI)) && settings.m_useReverseAPI) ||
            (settingsKeys.contains("reverseAPIAddress") && (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress)) ||
            (settingsKeys.contains("reverseAPIPort") && (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort)) ||
            (settingsKeys.contains("reverseAPIDeviceIndex") && (m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex)) ||
            (settingsKeys.contains("reverseAPIChannelIndex") && (m_settings.m_reverseAPIChannelIndex != settings.m_reverseAPIChannelIndex));
        webapiReverseSendSettings(settingsKeys, settings, fullUpdate || force);
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

QByteArray MorseMod::serialize() const
{
    return m_settings.serialize();
}

bool MorseMod::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureMorseMod *msg = MsgConfigureMorseMod::create(QStringList(), m_settings, true);
    m_inputMessageQueue.push(msg);

    return success;
}

void MorseMod::sendSampleRateToDemodAnalyzer()
{
    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "reportdemod", pipes);

    for (const auto& pipe : pipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        MainCore::MsgChannelDemodReport *msg = MainCore::MsgChannelDemodReport::create(
            this, getSourceChannelSampleRate());
        messageQueue->push(msg);
    }
}

int MorseMod::webapiSettingsGet(
    SWGSDRangel::SWGChannelSettings& response,
    QString& errorMessage)
{
    (void)response;
    errorMessage = "MorseMod: settings get not implemented";
    return 501;
}

int MorseMod::webapiWorkspaceGet(
    SWGSDRangel::SWGWorkspaceInfo& response,
    QString& errorMessage)
{
    (void)errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int MorseMod::webapiSettingsPutPatch(
    bool force,
    const QStringList& channelSettingsKeys,
    SWGSDRangel::SWGChannelSettings& response,
    QString& errorMessage)
{
    (void)force;
    (void)channelSettingsKeys;
    (void)response;
    errorMessage = "MorseMod: settings put/patch not implemented";
    return 501;
}

int MorseMod::webapiReportGet(
    SWGSDRangel::SWGChannelReport& response,
    QString& errorMessage)
{
    (void)response;
    errorMessage = "MorseMod: report get not implemented";
    return 501;
}

int MorseMod::webapiActionsPost(
    const QStringList& channelActionsKeys,
    SWGSDRangel::SWGChannelActions& query,
    QString& errorMessage)
{
    (void)channelActionsKeys;
    (void)query;
    errorMessage = "MorseMod: actions post not implemented";
    return 501;
}

void MorseMod::webapiReverseSendSettings(
    const QList<QString>& channelSettingsKeys,
    const MorseModSettings& settings,
    bool force)
{
    // Stub: reverse API not implemented for Morse plugin
    (void)channelSettingsKeys;
    (void)settings;
    (void)force;
}

void MorseMod::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    const QList<QString>& channelSettingsKeys,
    const MorseModSettings& settings,
    bool force)
{
    // Stub: channel settings pipes not implemented for Morse plugin
    (void)pipes;
    (void)channelSettingsKeys;
    (void)settings;
    (void)force;
}

double MorseMod::getMagSq() const
{
    return m_basebandSource->getMagSq();
}

void MorseMod::setLevelMeter(QObject *levelMeter)
{
    connect(m_basebandSource, SIGNAL(levelChanged(qreal, qreal, int)), levelMeter, SLOT(levelChanged(qreal, qreal, int)));
}

uint32_t MorseMod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSinkStreams();
}

int MorseMod::getSourceChannelSampleRate() const
{
    return m_basebandSource->getSourceChannelSampleRate();
}

void MorseMod::setMessageQueueToGUI(MessageQueue* queue)
{
    ChannelAPI::setMessageQueueToGUI(queue);
    m_basebandSource->setMessageQueueToGUI(queue);
}

void MorseMod::openUDP(const MorseModSettings& settings)
{
    closeUDP();
    m_udpSocket = new QUdpSocket();
    if (!m_udpSocket->bind(QHostAddress(settings.m_udpAddress), settings.m_udpPort))
    {
        qCritical() << "MorseMod::openUDP: failed to bind UDP socket"
                    << settings.m_udpAddress << ":" << settings.m_udpPort
                    << "- " << m_udpSocket->errorString();
    }
    else
    {
        qDebug() << "MorseMod::openUDP: bound to" << settings.m_udpAddress << ":" << settings.m_udpPort;
    }
    connect(m_udpSocket, SIGNAL(readyRead()), this, SLOT(udpRx()));
}

void MorseMod::closeUDP()
{
    if (m_udpSocket)
    {
        disconnect(m_udpSocket, SIGNAL(readyRead()), this, SLOT(udpRx()));
        delete m_udpSocket;
        m_udpSocket = nullptr;
    }
}

void MorseMod::udpRx()
{
    while (m_udpSocket->hasPendingDatagrams())
    {
        QNetworkDatagram datagram = m_udpSocket->receiveDatagram();
        MsgTXText *msg = MsgTXText::create(QString(datagram.data()));
        m_basebandSource->getInputMessageQueue()->push(msg);
    }
}

void MorseMod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "MorseMod::networkManagerFinished: error(" << (int)replyError
                   << "): " << replyError
                   << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("MorseMod::networkManagerFinished: reply: %s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
