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

#ifndef PLUGINS_CHANNELTX_MODCW_CWMODSOURCE_H
#define PLUGINS_CHANNELTX_MODCW_CWMODSOURCE_H

#include <QMutex>
#include <QList>

#include "dsp/channelsamplesource.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "util/movingaverage.h"

#include "cwmodsettings.h"

class BasebandSampleSink;
class ChannelAPI;
class MessageQueue;

/**
 * DSP signal source for the CW (Morse code) modulator.
 *
 * Encodes text as Morse code and generates a keyed carrier at the configured
 * tone frequency. Smooth keying (raised-cosine envelope) prevents key clicks.
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
    void setMessageQueueToGUI(MessageQueue *messageQueue) { m_messageQueueToGUI = messageQueue; }
    void setSpectrumSink(BasebandSampleSink *sampleSink) { m_spectrumSink = sampleSink; }
    void applySettings(const QStringList& settingsKeys, const CWModSettings& settings, bool force = false);
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void addTXText(const QString& text);
    void setChannel(ChannelAPI *channel) { m_channel = channel; }
    int getChannelSampleRate() const { return m_channelSampleRate; }

private:
    /** Represents one Morse code element (dot, dash, or silence gap). */
    struct MorseElement {
        bool key;   ///< true = carrier on, false = carrier off
        int units;  ///< Duration in Morse time units
    };

    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    CWModSettings m_settings;
    ChannelAPI *m_channel;

    NCO m_carrierNco;
    Real m_linearGain;
    Complex m_modSample;

    // Keying envelope for smooth key shaping (raised cosine ramp)
    float m_envelope;           ///< Current envelope amplitude [0..1]
    float m_envelopeStep;       ///< Step per sample during ramp-up/down
    bool m_keyDown;             ///< True when key should be pressed
    int m_samplesPerUnit;       ///< Samples per Morse time unit
    int m_unitSampleCount;      ///< Samples remaining in current element

    QList<MorseElement> m_morseQueue;   ///< Queue of Morse elements to transmit
    int m_repeatsDone;                  ///< Number of completed repetitions

    BasebandSampleSink *m_spectrumSink;
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
    static const int m_levelNbSamples = 480;

    MessageQueue *m_messageQueueToGUI;

    // Morse code table (A-Z, 0-9, punctuation)
    static const char * const m_morseTable[];

    void encodeText(const QString& text);
    void buildMorseSequence(char c, QList<MorseElement>& out);
    void modulateSample();
    void calculateLevel(Real& sample);
    void sampleToSpectrum(Complex sample);
    void updateSamplesPerUnit();
};

#endif // PLUGINS_CHANNELTX_MODCW_CWMODSOURCE_H
