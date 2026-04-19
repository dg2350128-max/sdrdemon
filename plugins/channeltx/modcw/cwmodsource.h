///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 SDRangel Contributors                                      //
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

#ifndef PLUGINS_CHANNELTX_MODCW_CWMODSOURCE_H
#define PLUGINS_CHANNELTX_MODCW_CWMODSOURCE_H

#include <QMutex>
#include <QList>
#include <QDebug>

#include "dsp/channelsamplesource.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/firfilter.h"
#include "util/movingaverage.h"

#include "cwmodsettings.h"

class BasebandSampleSink;
class ChannelAPI;

/**
 * DSP source for the CW (Continuous Wave / Morse) modulator.
 *
 * Converts text to Morse code (via the Morse utility) and produces an
 * On-Off-Keyed carrier at the configured channel sample rate.
 */
class CWModSource : public ChannelSampleSource
{
public:
    CWModSource();
    virtual ~CWModSource();

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

    void applySettings(const QStringList& settingsKeys, const CWModSettings& settings, bool force = false);
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);

    void addTXText(const QString& text);
    void setChannel(ChannelAPI *channel) { m_channel = channel; }
    int getChannelSampleRate() const { return m_channelSampleRate; }

    /** Returns the Morse representation of the current text (for display). */
    static QString textToMorse(const QString& text);

private:
    /** A single keyed element in the CW transmission. */
    struct CWElement {
        bool m_isOn;   //!< true = carrier on (dit/dah), false = silence
        int  m_units;  //!< Duration in Morse unit multiples (1 unit = 1 dit)
    };

    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    CWModSettings m_settings;
    ChannelAPI *m_channel;

    NCO m_carrierNco;
    Real m_linearGain;
    Complex m_modSample;

    BasebandSampleSink* m_spectrumSink;
    SampleVector m_specSampleBuffer;
    static const int m_specSampleBufferSize = 256;
    int m_specSampleBufferIndex;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;
    bool m_interpolatorConsumed;
    int m_spectrumRate;

    double m_magsq;
    MovingAverageUtil<double, double, 16> m_movingAverage;

    quint32 m_levelCalcCount;
    qreal m_rmsLevel;
    qreal m_peakLevelOut;
    Real m_peakLevel;
    Real m_levelSum;

    static const int m_levelNbSamples = 480; // every 10ms at 48k Sa/s

    // CW state machine
    QString m_pendingText;            //!< Text queued for transmission
    QList<CWElement> m_cwSequence;    //!< Encoded CW element sequence for current text
    int m_seqIndex;                   //!< Current position within m_cwSequence
    int m_samplesLeft;                //!< Remaining samples in the current element
    int m_samplesPerUnit;             //!< Samples per single Morse unit (dit duration)
    bool m_carrierOn;                 //!< Current carrier state

    // Repeat counters
    int m_transmitCount;              //!< Number of transmissions remaining
    bool m_transmitting;              //!< Transmission in progress

    MessageQueue* m_messageQueueToGUI;

    MessageQueue* getMessageQueueToGUI() { return m_messageQueueToGUI; }

    void encodeMorse(const QString& text);
    void nextElement();
    void calculateLevel(Real& sample);
    void sampleToSpectrum(Complex sample);
    void modulateSample();
};

#endif // PLUGINS_CHANNELTX_MODCW_CWMODSOURCE_H
