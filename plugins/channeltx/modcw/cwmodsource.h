///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 SDRDemon contributors                                     //
//                                                                               //
// Morse Encoder (CW) modulator plugin — OOK sample source                      //
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
#include <QDebug>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QAtomicInt>

#include "dsp/channelsamplesource.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/firfilter.h"
#include "util/movingaverage.h"
#include "util/morse.h"

#include "cwmodsettings.h"

class BasebandSampleSink;
class ChannelAPI;

/**
 * OOK (On-Off Keying) sample source that encodes text as Morse code.
 *
 * The Morse timing follows the PARIS standard:
 *   - dot     = 1 unit on
 *   - dash    = 3 units on
 *   - inter-element gap    = 1 unit off  (between dots/dashes)
 *   - inter-character gap  = 3 units off (3 − already-counted 1 = 2 extra)
 *   - inter-word gap       = 7 units off (7 − 1 = 6 extra)
 *
 * At WPM words per minute (PARIS = 50 units/word):
 *   unit_duration_samples = sample_rate * (1.2 / WPM)
 *
 * An optional USB serial CW keyer is supported: when enabled, the source
 * monitors the CTS line of a QSerialPort and turns the carrier on/off
 * directly.  Bytes received on the serial port are also accepted as raw
 * Morse key events (any non-zero byte = key down, zero byte = key up).
 */
class CWModSource : public ChannelSampleSource
{
public:
    CWModSource();
    ~CWModSource() override;

    void pull(SampleVector::iterator begin, unsigned int nbSamples) override;
    void pullOne(Sample& sample) override;
    void prefetch(unsigned int nbSamples) override { (void)nbSamples; }

    double getMagSq() const { return m_magsq; }
    void getLevels(qreal& rmsLevel, qreal& peakLevel, int& numSamples) const
    {
        rmsLevel   = m_rmsLevel;
        peakLevel  = m_peakLevelOut;
        numSamples = m_levelNbSamples;
    }

    void setMessageQueueToGUI(MessageQueue* queue) { m_messageQueueToGUI = queue; }
    void setSpectrumSink(BasebandSampleSink* sink)  { m_spectrumSink = sink; }
    void setChannel(ChannelAPI* channel)            { m_channel = channel; }

    void applySettings(const QStringList& settingsKeys, const CWModSettings& settings, bool force = false);
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);

    /** Enqueue text to be converted to Morse and transmitted. */
    void addTXText(const QString& text);

    /** Directly key the transmitter on/off (USB keyer input). */
    void setKeyerState(bool keyDown);

    int getChannelSampleRate() const { return m_channelSampleRate; }

private:
    // ---------- types --------------------------------------------------------

    /** A single on/off interval expressed in samples remaining. */
    struct MorseSymbol {
        bool    on;      ///< true = carrier on, false = silence
        int     samples; ///< number of samples this state lasts
    };

    // ---------- channel / rate state -----------------------------------------
    int  m_channelSampleRate;
    int  m_channelFrequencyOffset;
    int  m_spectrumRate;
    CWModSettings m_settings;
    ChannelAPI* m_channel;

    // ---------- signal generation --------------------------------------------
    NCO    m_carrierNco;
    Real   m_linearGain;
    Complex m_modSample;
    bool   m_carrierOn;          ///< current OOK state

    // ---------- Morse symbol queue -------------------------------------------
    QList<MorseSymbol> m_symbolQueue;
    int   m_samplesRemainingInSymbol; ///< countdown for current symbol

    QString m_textToTransmit;        ///< text waiting to be converted

    // ---------- USB keyer (runs on GUI/timer thread; atomic flag used) --------
    QSerialPort*    m_serialKeyer;
    QAtomicInt      m_keyerDown;     ///< 1 = key pressed (carrier on), 0 = up

    // ---------- spectrum sink / interpolator ---------------------------------
    BasebandSampleSink* m_spectrumSink;
    SampleVector        m_specSampleBuffer;
    static const int    m_specSampleBufferSize = 256;
    int                 m_specSampleBufferIndex;
    Interpolator        m_interpolator;
    Real                m_interpolatorDistance;
    Real                m_interpolatorDistanceRemain;
    bool                m_interpolatorConsumed;

    // ---------- low-pass filter ----------------------------------------------
    Lowpass<Complex> m_lowpass;

    // ---------- level measurement --------------------------------------------
    double  m_magsq;
    MovingAverageUtil<double, double, 16> m_movingAverage;
    quint32 m_levelCalcCount;
    qreal   m_rmsLevel;
    qreal   m_peakLevelOut;
    Real    m_peakLevel;
    Real    m_levelSum;
    static const int m_levelNbSamples = 480;

    MessageQueue* m_messageQueueToGUI;

    // ---------- helpers ------------------------------------------------------
    void enqueueText(const QString& text);
    void advanceSymbol();
    void modulateSample();
    void calculateLevel(Real& sample);
    void sampleToSpectrum(Complex sample);
    void openKeyer(const QString& portName);
    void closeKeyer();
};

#endif // INCLUDE_CWMODSOURCE_H
