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

#include <cmath>
#include <algorithm>
#include <limits>

#include <QDebug>

#include "dsp/basebandsamplesink.h"
#include "dsp/datafifo.h"
#include "morsemod.h"
#include "morsemodsource.h"
#include "util/messagequeue.h"
#include "maincore.h"

// ---------------------------------------------------------------------------
// Static Morse code table
// ---------------------------------------------------------------------------

const QMap<QChar, QVector<int>>& MorseModSource::morseTable()
{
    static const QMap<QChar, QVector<int>> table = {
        { 'A', {DIT, DAH} },
        { 'B', {DAH, DIT, DIT, DIT} },
        { 'C', {DAH, DIT, DAH, DIT} },
        { 'D', {DAH, DIT, DIT} },
        { 'E', {DIT} },
        { 'F', {DIT, DIT, DAH, DIT} },
        { 'G', {DAH, DAH, DIT} },
        { 'H', {DIT, DIT, DIT, DIT} },
        { 'I', {DIT, DIT} },
        { 'J', {DIT, DAH, DAH, DAH} },
        { 'K', {DAH, DIT, DAH} },
        { 'L', {DIT, DAH, DIT, DIT} },
        { 'M', {DAH, DAH} },
        { 'N', {DAH, DIT} },
        { 'O', {DAH, DAH, DAH} },
        { 'P', {DIT, DAH, DAH, DIT} },
        { 'Q', {DAH, DAH, DIT, DAH} },
        { 'R', {DIT, DAH, DIT} },
        { 'S', {DIT, DIT, DIT} },
        { 'T', {DAH} },
        { 'U', {DIT, DIT, DAH} },
        { 'V', {DIT, DIT, DIT, DAH} },
        { 'W', {DIT, DAH, DAH} },
        { 'X', {DAH, DIT, DIT, DAH} },
        { 'Y', {DAH, DIT, DAH, DAH} },
        { 'Z', {DAH, DAH, DIT, DIT} },
        { '0', {DAH, DAH, DAH, DAH, DAH} },
        { '1', {DIT, DAH, DAH, DAH, DAH} },
        { '2', {DIT, DIT, DAH, DAH, DAH} },
        { '3', {DIT, DIT, DIT, DAH, DAH} },
        { '4', {DIT, DIT, DIT, DIT, DAH} },
        { '5', {DIT, DIT, DIT, DIT, DIT} },
        { '6', {DAH, DIT, DIT, DIT, DIT} },
        { '7', {DAH, DAH, DIT, DIT, DIT} },
        { '8', {DAH, DAH, DAH, DIT, DIT} },
        { '9', {DAH, DAH, DAH, DAH, DIT} },
        { '.', {DIT, DAH, DIT, DAH, DIT, DAH} },
        { ',', {DAH, DAH, DIT, DIT, DAH, DAH} },
        { '?', {DIT, DIT, DAH, DAH, DIT, DIT} },
        { '/', {DAH, DIT, DIT, DAH, DIT} },
        { '=', {DAH, DIT, DIT, DIT, DAH} },
        { '+', {DIT, DAH, DIT, DAH, DIT} },
        { '-', {DAH, DIT, DIT, DIT, DIT, DAH} },
    };
    return table;
}

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------

MorseModSource::MorseModSource() :
    m_channelSampleRate(48000),
    m_channelFrequencyOffset(0),
    m_spectrumRate(2000),
    m_cwPhase(0.0),
    m_linearGain(1.0f),
    m_envelope(0.0f),
    m_spectrumSink(nullptr),
    m_specSampleBufferIndex(0),
    m_magsq(0.0),
    m_levelCalcCount(0),
    m_peakLevel(0.0f),
    m_levelSum(0.0f),
    m_symIdx(0),
    m_currentSymbol(IDLE),
    m_currentSymbolSamples(1),
    m_sampleIdx(0),
    m_samplesPerUnit(1),
    m_rampSamples(1),
    m_messageQueueToGUI(nullptr)
{
    m_demodBuffer.resize(1 << 12);
    m_demodBufferFill = 0;

    m_specSampleBuffer.resize(m_specSampleBufferSize);
    m_interpolatorDistanceRemain = 0;
    m_interpolatorConsumed = false;
    m_interpolatorDistance = (Real)m_channelSampleRate / (Real)m_spectrumRate;
    m_interpolator.create(48, m_spectrumRate, m_spectrumRate / 2.2, 3.0);

    applySettings(QStringList(), m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

MorseModSource::~MorseModSource()
{
}

// ---------------------------------------------------------------------------
// Sample generation
// ---------------------------------------------------------------------------

void MorseModSource::pull(SampleVector::iterator begin, unsigned int nbSamples)
{
    std::for_each(
        begin,
        begin + nbSamples,
        [this](Sample& s) { pullOne(s); }
    );
}

void MorseModSource::pullOne(Sample& sample)
{
    if (m_settings.m_channelMute)
    {
        sample.m_real = 0.0f;
        sample.m_imag = 0.0f;
        return;
    }

    modulateSample();

    Complex ci = m_modSample;
    ci *= m_carrierNco.nextIQ();

    double magsq = ci.real() * ci.real() + ci.imag() * ci.imag();
    m_movingAverage(magsq);
    m_magsq = m_movingAverage.asDouble();

    sample.m_real = (FixReal)(ci.real() * SDR_TX_SCALEF);
    sample.m_imag = (FixReal)(ci.imag() * SDR_TX_SCALEF);
}

void MorseModSource::sampleToSpectrum(Complex sample)
{
    if (m_spectrumSink)
    {
        Complex out;
        if (m_interpolator.decimate(&m_interpolatorDistanceRemain, sample, &out))
        {
            m_interpolatorDistanceRemain += m_interpolatorDistance;
            Real r = std::real(out) * SDR_TX_SCALEF;
            Real i = std::imag(out) * SDR_TX_SCALEF;
            m_specSampleBuffer[m_specSampleBufferIndex++] = Sample(r, i);
            if (m_specSampleBufferIndex == m_specSampleBufferSize)
            {
                m_spectrumSink->feed(m_specSampleBuffer.begin(), m_specSampleBuffer.end(), false);
                m_specSampleBufferIndex = 0;
            }
        }
    }
}

void MorseModSource::modulateSample()
{
    // Advance to next symbol when current one is finished
    if (m_sampleIdx == 0)
    {
        if (m_symIdx >= m_symbols.size())
        {
            // Symbol list exhausted - refill from text or go idle
            m_symbols.clear();
            m_symIdx = 0;

            if (!m_textToTransmit.isEmpty())
            {
                QChar ch = m_textToTransmit.at(0);
                m_textToTransmit.remove(0, 1);
                encodeChar(ch);
            }
            else
            {
                // Nothing to send - emit one idle unit then notify GUI
                m_symbols.append(IDLE);
                if (m_messageQueueToGUI) {
                    MorseMod::MsgReportTx *msg = MorseMod::MsgReportTx::create(QString(), 0);
                    m_messageQueueToGUI->push(msg);
                }
            }
        }

        m_currentSymbol = m_symbols[m_symIdx];
        m_currentSymbolSamples = symbolSamples(m_currentSymbol);
        m_symIdx++;
    }

    m_sampleIdx++;
    if (m_sampleIdx >= m_currentSymbolSamples) {
        m_sampleIdx = 0;
    }

    bool isKeyDown = (m_currentSymbol == DIT || m_currentSymbol == DAH);

    // Smooth envelope ramp toward key state to avoid clicks
    Real target = isKeyDown ? 1.0f : 0.0f;
    Real step = (m_rampSamples > 0) ? (1.0f / (Real)m_rampSamples) : 1.0f;

    if (m_envelope < target) {
        m_envelope = std::min(target, m_envelope + step);
    } else {
        m_envelope = std::max(target, m_envelope - step);
    }

    if (m_envelope > 0.0f)
    {
        m_cwPhase += 2.0 * M_PI * m_settings.m_toneFrequency / (double)m_channelSampleRate;
        if (m_cwPhase > M_PI) {
            m_cwPhase -= 2.0 * M_PI;
        }
        m_modSample.real(m_linearGain * m_envelope * (Real)cos(m_cwPhase));
        m_modSample.imag(m_linearGain * m_envelope * (Real)sin(m_cwPhase));
    }
    else
    {
        m_modSample = Complex(0.0f, 0.0f);
    }

    sampleToSpectrum(m_modSample);

    Real s = std::real(m_modSample);
    calculateLevel(s);

    m_demodBuffer[m_demodBufferFill] = isKeyDown
        ? std::numeric_limits<int16_t>::max()
        : 0;
    m_demodBufferFill++;

    if (m_demodBufferFill >= m_demodBuffer.size())
    {
        QList<ObjectPipe*> dataPipes;
        MainCore::instance()->getDataPipes().getDataPipes(m_channel, "demod", dataPipes);

        for (const auto& pipe : dataPipes)
        {
            DataFifo *fifo = qobject_cast<DataFifo*>(pipe->m_element);
            if (fifo) {
                fifo->write((quint8*)&m_demodBuffer[0], m_demodBuffer.size() * sizeof(qint16), DataFifo::DataTypeI16);
            }
        }

        m_demodBufferFill = 0;
    }
}

void MorseModSource::calculateLevel(Real& sample)
{
    if (m_levelCalcCount < (quint32)m_levelNbSamples)
    {
        m_peakLevel = std::max(std::fabs(m_peakLevel), sample);
        m_levelSum += sample * sample;
        m_levelCalcCount++;
    }
    else
    {
        m_rmsLevel = std::sqrt(m_levelSum / m_levelNbSamples);
        m_peakLevelOut = m_peakLevel;
        m_peakLevel = 0.0f;
        m_levelSum = 0.0f;
        m_levelCalcCount = 0;
    }
}

// ---------------------------------------------------------------------------
// Morse encoding
// ---------------------------------------------------------------------------

int MorseModSource::symbolSamples(int symbol) const
{
    switch (symbol)
    {
    case DIT:       return m_samplesPerUnit;
    case DAH:       return 3 * m_samplesPerUnit;
    case ELEM_GAP:  return m_samplesPerUnit;
    case CHAR_GAP:  return 3 * m_samplesPerUnit;
    case WORD_GAP:  return 7 * m_samplesPerUnit;
    case IDLE:
    default:        return m_samplesPerUnit;
    }
}

void MorseModSource::encodeChar(QChar ch)
{
    if (ch == ' ')
    {
        // Replace trailing CHAR_GAP with WORD_GAP, or append WORD_GAP
        if (!m_symbols.isEmpty() && m_symbols.last() == CHAR_GAP) {
            m_symbols.last() = WORD_GAP;
        } else {
            m_symbols.append(WORD_GAP);
        }

        if (m_messageQueueToGUI) {
            MorseMod::MsgReportTx *msg = MorseMod::MsgReportTx::create(QString(" "), m_textToTransmit.size());
            m_messageQueueToGUI->push(msg);
        }
        return;
    }

    QChar upper = ch.toUpper();
    const QMap<QChar, QVector<int>>& table = morseTable();

    if (!table.contains(upper)) {
        return;  // Unknown character - skip silently
    }

    const QVector<int>& code = table[upper];

    for (int i = 0; i < code.size(); i++)
    {
        m_symbols.append(code[i]);
        if (i < code.size() - 1) {
            m_symbols.append(ELEM_GAP);
        }
    }
    m_symbols.append(CHAR_GAP);

    if (m_messageQueueToGUI) {
        MorseMod::MsgReportTx *msg = MorseMod::MsgReportTx::create(QString(upper), m_textToTransmit.size());
        m_messageQueueToGUI->push(msg);
    }
}

void MorseModSource::addTXText(const QString& data)
{
    int count = m_settings.m_repeat ? m_settings.m_repeatCount : 1;
    for (int i = 0; i < count; i++) {
        m_textToTransmit.append(data);
    }
}

// ---------------------------------------------------------------------------
// Settings application
// ---------------------------------------------------------------------------

void MorseModSource::applySettings(const QStringList& settingsKeys, const MorseModSettings& settings, bool force)
{
    if ((settingsKeys.contains("wpm") && (settings.m_wpm != m_settings.m_wpm)) || force)
    {
        // PARIS standard: 1 WPM = 50 dit-units per minute
        m_samplesPerUnit = (int)((double)m_channelSampleRate * 60.0 / (50.0 * settings.m_wpm));
        if (m_samplesPerUnit < 1) {
            m_samplesPerUnit = 1;
        }
        qDebug() << "MorseModSource::applySettings: m_samplesPerUnit=" << m_samplesPerUnit
                 << " wpm=" << settings.m_wpm;
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    m_linearGain = powf(10.0f, m_settings.m_gain / 20.0f);
}

void MorseModSource::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "MorseModSource::applyChannelSettings:"
             << " channelSampleRate=" << channelSampleRate
             << " channelFrequencyOffset=" << channelFrequencyOffset;

    if ((channelFrequencyOffset != m_channelFrequencyOffset)
        || (channelSampleRate != m_channelSampleRate) || force)
    {
        m_carrierNco.setFreq(channelFrequencyOffset, channelSampleRate);
    }

    if ((m_channelSampleRate != channelSampleRate) || force)
    {
        m_interpolatorDistanceRemain = 0;
        m_interpolatorConsumed = false;
        m_interpolatorDistance = (Real)channelSampleRate / (Real)m_spectrumRate;
        m_interpolator.create(48, m_spectrumRate, m_spectrumRate / 2.2, 3.0);
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;

    // Recalculate timing parameters
    m_samplesPerUnit = (int)((double)m_channelSampleRate * 60.0 / (50.0 * m_settings.m_wpm));
    if (m_samplesPerUnit < 1) {
        m_samplesPerUnit = 1;
    }

    // 5 ms ramp to avoid key clicks
    m_rampSamples = channelSampleRate * 5 / 1000;
    if (m_rampSamples < 1) {
        m_rampSamples = 1;
    }

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_channel, "reportdemod", pipes);
    for (const auto& pipe : pipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        MainCore::MsgChannelDemodReport *msg = MainCore::MsgChannelDemodReport::create(m_channel, m_channelSampleRate);
        messageQueue->push(msg);
    }
}
