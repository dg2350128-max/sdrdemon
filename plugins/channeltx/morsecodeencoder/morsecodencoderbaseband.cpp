///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 SDRDemon Project                                           //
//                                                                               //
// Morse Code Encoder — baseband implementation                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
///////////////////////////////////////////////////////////////////////////////////

#include <QDebug>

#include "dsp/upchannelizer.h"
#include "dsp/dspcommands.h"

#include "morsecodencoderbaseband.h"
#include "morsecodeencoder.h"

MESSAGE_CLASS_DEFINITION(MorseCodeEncoderBaseband::MsgConfigureMorseCodeEncoderBaseband, Message)

MorseCodeEncoderBaseband::MorseCodeEncoderBaseband()
{
    m_sampleFifo.resize(SampleSourceFifo::getSizePolicy(48000));
    m_channelizer = new UpChannelizer(&m_source);

    QObject::connect(
        &m_sampleFifo,
        &SampleSourceFifo::dataRead,
        this,
        &MorseCodeEncoderBaseband::handleData,
        Qt::QueuedConnection
    );

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

MorseCodeEncoderBaseband::~MorseCodeEncoderBaseband()
{
    delete m_channelizer;
}

void MorseCodeEncoderBaseband::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_sampleFifo.reset();
}

void MorseCodeEncoderBaseband::setChannel(ChannelAPI *channel)
{
    m_source.setChannel(channel);
}

void MorseCodeEncoderBaseband::pull(const SampleVector::iterator& begin, unsigned int nbSamples)
{
    unsigned int part1Begin, part1End, part2Begin, part2End;
    m_sampleFifo.read(nbSamples, part1Begin, part1End, part2Begin, part2End);
    SampleVector& data = m_sampleFifo.getData();

    if (part1Begin != part1End) {
        std::copy(data.begin() + part1Begin, data.begin() + part1End, begin);
    }

    unsigned int shift = part1End - part1Begin;

    if (part2Begin != part2End) {
        std::copy(data.begin() + part2Begin, data.begin() + part2End, begin + shift);
    }
}

void MorseCodeEncoderBaseband::handleData()
{
    QMutexLocker mutexLocker(&m_mutex);
    SampleVector& data = m_sampleFifo.getData();
    unsigned int ipart1begin, ipart1end, ipart2begin, ipart2end;
    qreal rmsLevel, peakLevel;
    int numSamples;

    unsigned int remainder = m_sampleFifo.remainder();

    while ((remainder > 0) && (m_inputMessageQueue.size() == 0))
    {
        m_sampleFifo.write(remainder, ipart1begin, ipart1end, ipart2begin, ipart2end);

        if (ipart1begin != ipart1end) {
            processFifo(data, ipart1begin, ipart1end);
        }
        if (ipart2begin != ipart2end) {
            processFifo(data, ipart2begin, ipart2end);
        }

        remainder = m_sampleFifo.remainder();
    }

    m_source.getLevels(rmsLevel, peakLevel, numSamples);
    emit levelChanged(rmsLevel, peakLevel, numSamples);
}

void MorseCodeEncoderBaseband::processFifo(SampleVector& data, unsigned int iBegin, unsigned int iEnd)
{
    m_channelizer->prefetch(iEnd - iBegin);
    m_channelizer->pull(data.begin() + iBegin, iEnd - iBegin);
}

void MorseCodeEncoderBaseband::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

bool MorseCodeEncoderBaseband::handleMessage(const Message& cmd)
{
    if (MsgConfigureMorseCodeEncoderBaseband::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureMorseCodeEncoderBaseband& cfg = (MsgConfigureMorseCodeEncoderBaseband&) cmd;
        qDebug() << "MorseCodeEncoderBaseband::handleMessage: MsgConfigureMorseCodeEncoderBaseband";
        applySettings(cfg.getSettingsKeys(), cfg.getSettings(), cfg.getForce());
        return true;
    }
    else if (MorseCodeEncoder::MsgTx::match(cmd))
    {
        qDebug() << "MorseCodeEncoderBaseband::handleMessage: MsgTx";
        m_source.getCWKeyer().resetText();
        return true;
    }
    else if (MorseCodeEncoder::MsgTXText::match(cmd))
    {
        MorseCodeEncoder::MsgTXText& tx = (MorseCodeEncoder::MsgTXText&) cmd;
        m_source.enqueueMessage(tx.m_text);
        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        qDebug() << "MorseCodeEncoderBaseband::handleMessage: DSPSignalNotification: basebandSampleRate: " << notif.getSampleRate();
        m_sampleFifo.resize(SampleSourceFifo::getSizePolicy(notif.getSampleRate()));
        m_channelizer->setBasebandSampleRate(notif.getSampleRate());
        m_source.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());
        return true;
    }
    else
    {
        return false;
    }
}

void MorseCodeEncoderBaseband::applySettings(const QStringList& settingsKeys, const MorseCodeEncoderSettings& settings, bool force)
{
    if ((settingsKeys.contains("inputFrequencyOffset") && (settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset)) || force)
    {
        m_channelizer->setChannelization(48000, settings.m_inputFrequencyOffset);
        m_source.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());
    }

    m_source.applySettings(settingsKeys, settings, force);

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

int MorseCodeEncoderBaseband::getChannelSampleRate() const
{
    return m_channelizer->getChannelSampleRate();
}
