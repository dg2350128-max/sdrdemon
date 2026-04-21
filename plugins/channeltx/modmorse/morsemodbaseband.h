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

#ifndef INCLUDE_MORSEMODBASEBAND_H
#define INCLUDE_MORSEMODBASEBAND_H

#include <QObject>
#include <QRecursiveMutex>

#include "dsp/samplesourcefifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "morsemodsource.h"

class UpChannelizer;
class ChannelAPI;

/** Baseband processor for the Morse (CW) modulator. Runs in a dedicated thread. */
class MorseModBaseband : public QObject
{
    Q_OBJECT

public:
    /** Configure the baseband with a new settings snapshot. */
    class MsgConfigureMorseModBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const MorseModSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureMorseModBaseband* create(const QStringList& settingsKeys, const MorseModSettings& settings, bool force)
        {
            return new MsgConfigureMorseModBaseband(settingsKeys, settings, force);
        }

    private:
        MorseModSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureMorseModBaseband(const QStringList& settingsKeys, const MorseModSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        {}
    };

    MorseModBaseband();
    ~MorseModBaseband();
    void reset();
    void pull(const SampleVector::iterator& begin, unsigned int nbSamples);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void setMessageQueueToGUI(MessageQueue *messageQueue) { m_source.setMessageQueueToGUI(messageQueue); }
    double getMagSq() const { return m_source.getMagSq(); }
    int getChannelSampleRate() const;
    void setSpectrumSampleSink(BasebandSampleSink* sampleSink) { m_source.setSpectrumSink(sampleSink); }
    void setChannel(ChannelAPI *channel);
    int getSourceChannelSampleRate() const { return m_source.getChannelSampleRate(); }

signals:
    /**
     * Level changed.
     * @param rmsLevel  RMS level in range [0.0, 1.0]
     * @param peakLevel Peak level in range [0.0, 1.0]
     * @param numSamples Number of samples analyzed
     */
    void levelChanged(qreal rmsLevel, qreal peakLevel, int numSamples);

private:
    SampleSourceFifo m_sampleFifo;
    UpChannelizer *m_channelizer;
    MorseModSource m_source;
    MessageQueue m_inputMessageQueue;
    MorseModSettings m_settings;
    QRecursiveMutex m_mutex;

    void processFifo(SampleVector& data, unsigned int iBegin, unsigned int iEnd);
    bool handleMessage(const Message& cmd);
    void applySettings(const QStringList& settingsKeys, const MorseModSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData();
};

#endif // INCLUDE_MORSEMODBASEBAND_H
