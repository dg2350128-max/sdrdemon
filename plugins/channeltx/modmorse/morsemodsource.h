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

#ifndef INCLUDE_MORSEMSOURCE_H
#define INCLUDE_MORSEMSOURCE_H

#include <QMutex>
#include <QDebug>
#include <QMap>
#include <QVector>

#include "dsp/channelsamplesource.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "util/movingaverage.h"
#include "util/messagequeue.h"

#include "morsemodsettings.h"

class BasebandSampleSink;
class ChannelAPI;

/** Sample source that generates OOK-keyed CW (Morse code) I/Q samples. */
class MorseModSource : public ChannelSampleSource
{
public:
    /** Morse symbol types used in the internal symbol queue. */
    enum SymbolType {
        DIT = 0,       ///< Short element (dot)
        DAH = 1,       ///< Long element (dash)
        ELEM_GAP = 2,  ///< Gap between elements of the same character
        CHAR_GAP = 3,  ///< Gap between characters
        WORD_GAP = 4,  ///< Gap between words
        IDLE = 5       ///< Idle / silence
    };

    MorseModSource();
    virtual ~MorseModSource();

    virtual void pull(SampleVector::iterator begin, unsigned int nbSamples);
    virtual void pullOne(Sample& sample);
    virtual void prefetch(unsigned int nbSamples) { (void)nbSamples; }

    double getMagSq() const { return m_magsq; }
    void getLevels(qreal& rmsLevel, qreal& peakLevel, int& numSamples) const
    {
        rmsLevel = m_rmsLevel;
        peakLevel = m_peakLevelOut;
        numSamples = m_levelNbSamples;
    }
    void setMessageQueueToGUI(MessageQueue* messageQueue) { m_messageQueueToGUI = messageQueue; }
    void setSpectrumSink(BasebandSampleSink *sampleSink) { m_spectrumSink = sampleSink; }
    void applySettings(const QStringList& settingsKeys, const MorseModSettings& settings, bool force = false);
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void addTXText(const QString& data);
    void setChannel(ChannelAPI *channel) { m_channel = channel; }
    int getChannelSampleRate() const { return m_channelSampleRate; }

private:
    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    int m_spectrumRate;
    MorseModSettings m_settings;
    ChannelAPI *m_channel;

    NCO m_carrierNco;
    double m_cwPhase;     ///< Accumulated CW carrier phase (radians)
    Real m_linearGain;
    Real m_envelope;      ///< Smoothed key envelope [0..1]
    Complex m_modSample;

    // Symbol playback state
    QList<int> m_symbols;        ///< Pending Morse symbol queue
    int m_symIdx;                ///< Next symbol to play
    int m_currentSymbol;         ///< Currently playing symbol
    int m_currentSymbolSamples;  ///< Total samples for current symbol
    int m_sampleIdx;             ///< Sample counter within current symbol
    int m_samplesPerUnit;        ///< Samples per single Morse unit (dit length)
    int m_rampSamples;           ///< Rise/fall ramp length in samples (5 ms)

    QString m_textToTransmit;    ///< Text waiting to be encoded and transmitted

    BasebandSampleSink *m_spectrumSink;
    SampleVector m_specSampleBuffer;
    static const int m_specSampleBufferSize = 256;
    int m_specSampleBufferIndex;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;
    bool m_interpolatorConsumed;

    double m_magsq;
    MovingAverageUtil<double, double, 16> m_movingAverage;

    quint32 m_levelCalcCount;
    qreal m_rmsLevel;
    qreal m_peakLevelOut;
    Real m_peakLevel;
    Real m_levelSum;

    static const int m_levelNbSamples = 480;  ///< ~10 ms at 48 kSa/s

    QVector<qint16> m_demodBuffer;
    int m_demodBufferFill;

    MessageQueue *m_messageQueueToGUI;

    /** Encode a single character and append resulting symbols to m_symbols. */
    void encodeChar(QChar ch);
    /** Return the number of samples for a given symbol type. */
    int symbolSamples(int symbol) const;
    void calculateLevel(Real& sample);
    void modulateSample();
    void sampleToSpectrum(Complex sample);

    /** Returns the static Morse code lookup table (char -> dit/dah sequence). */
    static const QMap<QChar, QVector<int>>& morseTable();
};

#endif // INCLUDE_MORSEMSOURCE_H
