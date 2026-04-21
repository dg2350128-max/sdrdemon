///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 SDRDemon Project                                           //
//                                                                               //
// Morse Code Encoder — sample source (DSP layer)                                //
//                                                                               //
// Generates an OOK carrier keyed with Morse code.  Supports configurable        //
// inter-character and inter-word spacing multipliers.                            //
// Designed for HackRF One / HackRF Pro.                                         //
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

#ifndef INCLUDE_MORSECODENCODERSOURCE_H
#define INCLUDE_MORSECODENCODERSOURCE_H

#include <QMutex>
#include <QQueue>

#include "dsp/channelsamplesource.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/cwkeyer.h"
#include "util/movingaverage.h"
#include "util/morse.h"

#include "morsecodencodersettings.h"

class BasebandSampleSink;
class ChannelAPI;

/** DSP sample source for the Morse Code Encoder.
 *
 *  Generates an OOK (On-Off Keying) carrier at the configured tone frequency,
 *  keyed by Morse code symbols produced by the embedded CWKeyer.
 *  Messages from the GUI are queued and transmitted in order.
 */
class MorseCodeEncoderSource : public ChannelSampleSource
{
public:
    MorseCodeEncoderSource();
    virtual ~MorseCodeEncoderSource();

    virtual void pull(SampleVector::iterator begin, unsigned int nbSamples);
    virtual void pullOne(Sample& sample);
    virtual void prefetch(unsigned int nbSamples) { (void) nbSamples; }

    double getMagSq() const { return m_magsq; }
    void getLevels(qreal& rmsLevel, qreal& peakLevel, int& numSamples) const
    {
        rmsLevel = m_rmsLevel;
        peakLevel = m_peakLevelOut;
        numSamples = m_levelNbSamples;
    }
    void setMessageQueueToGUI(MessageQueue* messageQueue) { m_messageQueueToGUI = messageQueue; }
    void setSpectrumSink(BasebandSampleSink *sampleSink) { m_spectrumSink = sampleSink; }
    void applySettings(const QStringList& settingsKeys, const MorseCodeEncoderSettings& settings, bool force = false);
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void setChannel(ChannelAPI *channel) { m_channel = channel; }
    int getChannelSampleRate() const { return m_channelSampleRate; }
    CWKeyer& getCWKeyer() { return m_cwKeyer; }

    /** Queue a text message for transmission. */
    void enqueueMessage(const QString& text);

private:
    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    int m_spectrumRate;
    MorseCodeEncoderSettings m_settings;
    ChannelAPI *m_channel;

    NCO m_carrierNco;
    Real m_linearGain;

    CWKeyer m_cwKeyer;
    CWSmoother m_cwSmoother;

    QQueue<QString> m_txQueue;  //!< Pending messages
    QMutex m_queueMutex;

    // Spectrum feed
    BasebandSampleSink* m_spectrumSink;
    SampleVector m_specSampleBuffer;
    static const int m_specSampleBufferSize = 256;
    int m_specSampleBufferIndex;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;
    bool m_interpolatorConsumed;

    // Level meters
    double m_magsq;
    MovingAverageUtil<double, double, 16> m_movingAverage;
    quint32 m_levelCalcCount;
    qreal m_rmsLevel;
    qreal m_peakLevelOut;
    Real m_peakLevel;
    Real m_levelSum;
    static const int m_levelNbSamples = 480;

    QVector<qint16> m_demodBuffer;
    int m_demodBufferFill;

    MessageQueue* m_messageQueueToGUI;

    void modulateSample(Real& modSample);
    void calculateLevel(Real& sample);
    void sampleToSpectrum(Complex sample);
    void checkQueue();
};

#endif // INCLUDE_MORSECODENCODERSOURCE_H
