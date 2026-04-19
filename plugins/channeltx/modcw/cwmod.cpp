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

#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QThread>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"

#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "maincore.h"

#include "cwmodbaseband.h"
#include "cwmod.h"

MESSAGE_CLASS_DEFINITION(CWMod::MsgConfigureCWMod, Message)
MESSAGE_CLASS_DEFINITION(CWMod::MsgTx, Message)
MESSAGE_CLASS_DEFINITION(CWMod::MsgTXText, Message)

const char* const CWMod::m_channelIdURI = "sdrangel.channeltx.modcw";
const char* const CWMod::m_channelId    = "CWMod";

CWMod::CWMod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSource),
    m_deviceAPI(deviceAPI),
    m_spectrumVis(SDR_TX_SCALEF)
{
    setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSource = new CWModBaseband();
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
        &CWMod::networkManagerFinished
    );
}

CWMod::~CWMod()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &CWMod::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSourceAPI(this);
    m_deviceAPI->removeChannelSource(this, true);
    stop();
    delete m_basebandSource;
    delete m_thread;
}

void CWMod::setDeviceAPI(DeviceAPI *deviceAPI)
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

void CWMod::start()
{
    qDebug("CWMod::start");
    m_basebandSource->reset();
    m_thread->start();
}

void CWMod::stop()
{
    qDebug("CWMod::stop");
    m_thread->exit();
    m_thread->wait();
}

void CWMod::pull(SampleVector::iterator& begin, unsigned int nbSamples)
{
    m_basebandSource->pull(begin, nbSamples);
}

bool CWMod::handleMessage(const Message& cmd)
{
    if (MsgConfigureCWMod::match(cmd))
    {
        MsgConfigureCWMod& cfg = (MsgConfigureCWMod&) cmd;
        qDebug() << "CWMod::handleMessage: MsgConfigureCWMod";

        applySettings(cfg.getSettingsKeys(), cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (MsgTx::match(cmd))
    {
        MsgTx *msg = new MsgTx((const MsgTx&) cmd);
        m_basebandSource->getInputMessageQueue()->push(msg);

        return true;
    }
    else if (MsgTXText::match(cmd))
    {
        MsgTXText *msg = new MsgTXText((const MsgTXText&) cmd);
        m_basebandSource->getInputMessageQueue()->push(msg);

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        DSPSignalNotification *rep = new DSPSignalNotification(notif);
        qDebug() << "CWMod::handleMessage: DSPSignalNotification";
        m_basebandSource->getInputMessageQueue()->push(rep);

        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else if (MainCore::MsgChannelDemodQuery::match(cmd))
    {
        qDebug() << "CWMod::handleMessage: MsgChannelDemodQuery";
        sendSampleRateToDemodAnalyzer();

        return true;
    }
    else
    {
        return false;
    }
}

void CWMod::setCenterFrequency(qint64 frequency)
{
    CWModSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(QStringList("inputFrequencyOffset"), settings, false);

    if (m_guiMessageQueue)
    {
        MsgConfigureCWMod *msgToGUI = MsgConfigureCWMod::create(QStringList("inputFrequencyOffset"), settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void CWMod::applySettings(const QStringList& settingsKeys, const CWModSettings& settings, bool force)
{
    qDebug() << "CWMod::applySettings:" << settings.getDebugString(settingsKeys, force);

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

    CWModBaseband::MsgConfigureCWModBaseband *msg = CWModBaseband::MsgConfigureCWModBaseband::create(settingsKeys, settings, force);
    m_basebandSource->getInputMessageQueue()->push(msg);

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

QByteArray CWMod::serialize() const
{
    return m_settings.serialize();
}

bool CWMod::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureCWMod *msg = MsgConfigureCWMod::create(QStringList(), m_settings, true);
    m_inputMessageQueue.push(msg);

    return success;
}

void CWMod::sendSampleRateToDemodAnalyzer()
{
    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "reportdemod", pipes);

    for (const auto& pipe : pipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        MainCore::MsgChannelDemodReport *msg = MainCore::MsgChannelDemodReport::create(
            this,
            getSourceChannelSampleRate()
        );
        messageQueue->push(msg);
    }
}

int CWMod::webapiSettingsGet(
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage)
{
    (void) response;
    errorMessage = "CWMod WebAPI settings not yet supported";
    return 501;
}

int CWMod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int CWMod::webapiSettingsPutPatch(
                bool force,
                const QStringList& channelSettingsKeys,
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage)
{
    (void) force;
    (void) channelSettingsKeys;
    (void) response;
    errorMessage = "CWMod WebAPI settings not yet supported";
    return 501;
}

double CWMod::getMagSq() const
{
    return m_basebandSource->getMagSq();
}

void CWMod::setLevelMeter(QObject *levelMeter)
{
    connect(m_basebandSource, SIGNAL(levelChanged(qreal, qreal, int)), levelMeter, SLOT(levelChanged(qreal, qreal, int)));
}

uint32_t CWMod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSinkStreams();
}

int CWMod::getSourceChannelSampleRate() const
{
    return m_basebandSource->getSourceChannelSampleRate();
}

void CWMod::setMessageQueueToGUI(MessageQueue *queue)
{
    ChannelAPI::setMessageQueueToGUI(queue);
    m_basebandSource->setMessageQueueToGUI(queue);
}

void CWMod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "CWMod::networkManagerFinished: error(" << (int)replyError
                   << "): " << replyError
                   << ": " << reply->errorString();
    }

    reply->deleteLater();
}
