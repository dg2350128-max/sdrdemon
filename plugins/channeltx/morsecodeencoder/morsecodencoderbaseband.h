///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 SDRDemon Project                                           //
//                                                                               //
// Morse Code Encoder — baseband header                                          //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_MORSECODENCODERBASEBAND_H
#define INCLUDE_MORSECODENCODERBASEBAND_H

#include <QObject>
#include <QRecursiveMutex>

#include "dsp/samplesourcefifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "morsecodencodersource.h"

class UpChannelizer;
class ChannelAPI;

class MorseCodeEncoderBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureMorseCodeEncoderBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const MorseCodeEncoderSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureMorseCodeEncoderBaseband* create(const QStringList& settingsKeys, const MorseCodeEncoderSettings& settings, bool force)
        {
            return new MsgConfigureMorseCodeEncoderBaseband(settingsKeys, settings, force);
        }

    private:
        MorseCodeEncoderSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureMorseCodeEncoderBaseband(const QStringList& settingsKeys, const MorseCodeEncoderSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    MorseCodeEncoderBaseband();
    ~MorseCodeEncoderBaseband();
    void reset();
    void pull(const SampleVector::iterator& begin, unsigned int nbSamples);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void setMessageQueueToGUI(MessageQueue* messageQueue) { m_source.setMessageQueueToGUI(messageQueue); }
    double getMagSq() const { return m_source.getMagSq(); }
    int getChannelSampleRate() const;
    void setSpectrumSampleSink(BasebandSampleSink* sampleSink) { m_source.setSpectrumSink(sampleSink); }
    void setChannel(ChannelAPI *channel);
    CWKeyer& getCWKeyer() { return m_source.getCWKeyer(); }
    int getSourceChannelSampleRate() const { return m_source.getChannelSampleRate(); }
    void enqueueMessage(const QString& text) { m_source.enqueueMessage(text); }

signals:
    void levelChanged(qreal rmsLevel, qreal peakLevel, int numSamples);

private:
    SampleSourceFifo m_sampleFifo;
    UpChannelizer *m_channelizer;
    MorseCodeEncoderSource m_source;
    MessageQueue m_inputMessageQueue;
    MorseCodeEncoderSettings m_settings;
    QRecursiveMutex m_mutex;

    void processFifo(SampleVector& data, unsigned int iBegin, unsigned int iEnd);
    bool handleMessage(const Message& cmd);
    void applySettings(const QStringList& settingsKeys, const MorseCodeEncoderSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData();
};

#endif // INCLUDE_MORSECODENCODERBASEBAND_H
