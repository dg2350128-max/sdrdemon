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

#include <cmath>
#include <algorithm>
#include <QDebug>

#include "dsp/basebandsamplesink.h"
#include "util/morse.h"
#include "util/messagequeue.h"
#include "maincore.h"

#include "cwmod.h"
#include "cwmodsource.h"

CWModSource::CWModSource() :
    m_channelSampleRate(48000),
    m_channelFrequencyOffset(0),
    m_spectrumSink(nullptr),
    m_specSampleBufferIndex(0),
    m_spectrumRate(8000),
    m_magsq(0.0),
    m_levelCalcCount(0),
    m_peakLevel(0.0f),
    m_levelSum(0.0f),
    m_seqIndex(0),
    m_samplesLeft(0),
    m_samplesPerUnit(1),
    m_carrierOn(false),
    m_transmitCount(0),
    m_transmitting(false),
    m_messageQueueToGUI(nullptr)
{
    m_specSampleBuffer.resize(m_specSampleBufferSize);
    m_interpolatorDistanceRemain = 0;
    m_interpolatorConsumed = false;
    m_interpolatorDistance = (Real)m_channelSampleRate / (Real)m_spectrumRate;
    m_interpolator.create(48, m_spectrumRate, m_spectrumRate / 2.2, 3.0);

    applySettings(QStringList(), m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

CWModSource::~CWModSource()
{
}

void CWModSource::pull(SampleVector::iterator begin, unsigned int nbSamples)
{
    std::for_each(
        begin,
        begin + nbSamples,
        [this](Sample& s) {
            pullOne(s);
        }
    );
}

void CWModSource::pullOne(Sample& sample)
{
    if (m_settings.m_channelMute)
    {
        sample.m_real = 0.0f;
        sample.m_imag = 0.0f;
        return;
    }

    modulateSample();

    // Shift to carrier frequency
    Complex ci = m_modSample;
    ci *= m_carrierNco.nextIQ();

    // Calculate power
    double magsq = ci.real() * ci.real() + ci.imag() * ci.imag();
    m_movingAverage(magsq);
    m_magsq = m_movingAverage.asDouble();

    // Convert from float to fixed point
    sample.m_real = (FixReal)(ci.real() * SDR_TX_SCALEF);
    sample.m_imag = (FixReal)(ci.imag() * SDR_TX_SCALEF);
}

void CWModSource::sampleToSpectrum(Complex sample)
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

void CWModSource::modulateSample()
{
    // Advance CW state machine
    if (m_samplesLeft <= 0)
    {
        nextElement();
    }
    m_samplesLeft--;

    Real s;
    if (m_carrierOn && m_transmitting)
    {
        s = m_linearGain;
        m_modSample.real(s);
        m_modSample.imag(0.0f);
    }
    else
    {
        s = 0.0f;
        m_modSample.real(0.0f);
        m_modSample.imag(0.0f);
    }

    sampleToSpectrum(m_modSample);
    calculateLevel(s);
}

void CWModSource::nextElement()
{
    if (!m_transmitting || m_cwSequence.isEmpty())
    {
        // No active transmission — check for pending text
        if (!m_pendingText.isEmpty() && m_transmitCount != 0)
        {
            encodeMorse(m_pendingText);
            m_pendingText.clear();
            m_seqIndex = 0;
            m_transmitting = true;
            if (m_transmitCount > 0) {
                m_transmitCount--;
            }
        }
        else
        {
            m_carrierOn = false;
            m_samplesLeft = m_samplesPerUnit; // idle
            return;
        }
    }

    if (m_seqIndex < m_cwSequence.size())
    {
        const CWElement& elem = m_cwSequence.at(m_seqIndex);
        m_carrierOn = elem.m_isOn;
        m_samplesLeft = elem.m_units * m_samplesPerUnit;
        m_seqIndex++;
    }
    else
    {
        // Finished the current sequence
        m_transmitting = false;
        m_cwSequence.clear();
        m_carrierOn = false;

        // Add inter-message gap (7 units silence) before potential repeat
        m_samplesLeft = 7 * m_samplesPerUnit;

        // Notify GUI
        if (getMessageQueueToGUI())
        {
            CWMod::MsgReportTx* msg = CWMod::MsgReportTx::create();
            getMessageQueueToGUI()->push(msg);
        }
    }
}

void CWModSource::addTXText(const QString& text)
{
    m_pendingText = text;
    m_transmitCount = m_settings.m_repeat ? m_settings.m_repeatCount : 1;
    if (m_transmitCount == 0) {
        m_transmitCount = -1; // infinite
    }
}

// static
QString CWModSource::textToMorse(const QString& text)
{
    return Morse::toMorse(text);
}

void CWModSource::encodeMorse(const QString& text)
{
    m_cwSequence.clear();

    QString morse = Morse::toMorse(text.toUpper());

    // Parse the Morse string into CW elements.
    // Morse::toMorse() separates characters with " " and words with "   "
    // (1 separator space + the ' ' word-space char + 1 separator space = 3 spaces).
    // Dots/dashes within a character are adjacent.
    int i = 0;
    bool lastWasElement = false;

    while (i < morse.size())
    {
        const QChar c = morse.at(i);

        if (c == '.')
        {
            m_cwSequence.append({true, 1});   // dit on
            m_cwSequence.append({false, 1});  // element gap
            lastWasElement = true;
            i++;
        }
        else if (c == '-')
        {
            m_cwSequence.append({true, 3});   // dah on
            m_cwSequence.append({false, 1});  // element gap
            lastWasElement = true;
            i++;
        }
        else if (c == ' ')
        {
            // Count consecutive spaces
            int spaces = 0;
            while ((i < morse.size()) && (morse.at(i) == ' '))
            {
                spaces++;
                i++;
            }

            if (lastWasElement && !m_cwSequence.isEmpty())
            {
                // Remove the trailing element gap; replace with proper gap
                m_cwSequence.removeLast();
            }

            // 1 space = letter gap (3 units); >=3 spaces = word gap (7 units)
            if (spaces >= 3) {
                m_cwSequence.append({false, 7}); // word gap
            } else {
                m_cwSequence.append({false, 3}); // letter gap
            }

            lastWasElement = false;
        }
        else
        {
            i++; // skip unknown characters
        }
    }

    // Remove any trailing silence element
    while (!m_cwSequence.isEmpty() && !m_cwSequence.last().m_isOn) {
        m_cwSequence.removeLast();
    }
}

void CWModSource::calculateLevel(Real& sample)
{
    if (m_levelCalcCount < (quint32)m_levelNbSamples)
    {
        m_peakLevel = std::max(std::fabs(m_peakLevel), sample);
        m_levelSum += sample * sample;
        m_levelCalcCount++;
    }
    else
    {
        m_rmsLevel = sqrt(m_levelSum / m_levelNbSamples);
        m_peakLevelOut = m_peakLevel;
        m_peakLevel = 0.0f;
        m_levelSum = 0.0f;
        m_levelCalcCount = 0;
    }
}

void CWModSource::applySettings(const QStringList& settingsKeys, const CWModSettings& settings, bool force)
{
    if ((settingsKeys.contains("wpm") && (settings.m_wpm != m_settings.m_wpm)) || force)
    {
        // dit duration in ms = 1200 / WPM; samples per dit = sampleRate * ditDuration_ms / 1000
        m_samplesPerUnit = (m_channelSampleRate * 1200) / (settings.m_wpm * 1000);
        qDebug() << "CWModSource::applySettings: m_samplesPerUnit=" << m_samplesPerUnit
                 << " wpm=" << settings.m_wpm;
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    m_linearGain = powf(10.0f, m_settings.m_gain / 20.0f);
}

void CWModSource::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "CWModSource::applyChannelSettings:"
             << " channelSampleRate: " << channelSampleRate
             << " channelFrequencyOffset: " << channelFrequencyOffset;

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

        m_samplesPerUnit = (channelSampleRate * 1200) / (m_settings.m_wpm * 1000);
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_channel, "reportdemod", pipes);

    if (pipes.size() > 0)
    {
        for (const auto& pipe : pipes)
        {
            MessageQueue* messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            MainCore::MsgChannelDemodReport *msg = MainCore::MsgChannelDemodReport::create(m_channel, m_channelSampleRate);
            messageQueue->push(msg);
        }
    }
}
