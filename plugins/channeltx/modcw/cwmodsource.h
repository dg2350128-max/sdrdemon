///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 SDRDemon Project                                           //
//                                                                               //
// CW (Continuous Wave) Modulator — sample source (DSP layer)                    //
//                                                                               //
// Generates an OOK (On-Off Keying) carrier keyed with Morse code using the      //
// existing CWKeyer class.  Designed for use with HackRF One / HackRF Pro.       //
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

#ifndef INCLUDE_CWMODSOURCE_H
#define INCLUDE_CWMODSOURCE_H

#include <QMutex>

#include "dsp/channelsamplesource.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/firfilter.h"
#include "dsp/cwkeyer.h"
#include "util/movingaverage.h"

#include "cwmodsettings.h"

class BasebandSampleSink;
class ChannelAPI;

/** DSP sample source for the CW modulator.
 *
 *  Generates an OOK carrier at \c m_toneFrequency Hz, keyed on/off according
 *  to Morse code produced by the embedded CWKeyer.
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
    void setChannel(ChannelAPI *channel) { m_channel = channel; }
    int getChannelSampleRate() const { return m_channelSampleRate; }
    CWKeyer& getCWKeyer() { return m_cwKeyer; }

private:
    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    int m_spectrumRate;
    CWModSettings m_settings;
    ChannelAPI *m_channel;

    NCO m_carrierNco;       //!< Tone NCO
    Real m_linearGain;

    CWKeyer m_cwKeyer;      //!< Morse code timing engine
    CWSmoother m_cwSmoother; //!< Rise/fall shaping

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
};

#endif // INCLUDE_CWMODSOURCE_H
