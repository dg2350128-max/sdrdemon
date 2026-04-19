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

#include <cctype>
#include <cmath>
#include <algorithm>

#include <QDebug>

#include "dsp/basebandsamplesink.h"
#include "cwmodsource.h"

// International Morse code table (ITU-R M.1677-1).
// Each string is a sequence of '.' (dot) and '-' (dash) characters.
// Index 0 = 'A', 1 = 'B', ..., 25 = 'Z', 26 = '0', ..., 35 = '9'
const char * const CWModSource::m_morseTable[] = {
    ".-",    // A
    "-...",  // B
    "-.-.",  // C
    "-..",   // D
    ".",     // E
    "..-.",  // F
    "--.",   // G
    "....",  // H
    "..",    // I
    ".---",  // J
    "-.-",   // K
    ".-..",  // L
    "--",    // M
    "-.",    // N
    "---",   // O
    ".--.",  // P
    "--.-",  // Q
    ".-.",   // R
    "...",   // S
    "-",     // T
    "..-",   // U
    "...-",  // V
    ".--",   // W
    "-..-",  // X
    "-.--",  // Y
    "--..",  // Z
    "-----", // 0
    ".----", // 1
    "..---", // 2
    "...--", // 3
    "....-", // 4
    ".....", // 5
    "-....", // 6
    "--...", // 7
    "---..", // 8
    "----.", // 9
    nullptr
};

CWModSource::CWModSource() :
    m_channelSampleRate(48000),
    m_channelFrequencyOffset(0),
    m_channel(nullptr),
    m_linearGain(1.0f),
    m_envelope(0.0f),
    m_envelopeStep(0.0f),
    m_keyDown(false),
    m_samplesPerUnit(48000 / (50 * 20 / 60)), // 48 kHz, PARIS = 50 units/word, 20 WPM
    m_unitSampleCount(0),
    m_repeatsDone(0),
    m_spectrumSink(nullptr),
    m_specSampleBufferIndex(0),
    m_spectrumRate(2000),
    m_magsq(0.0),
    m_levelCalcCount(0),
    m_peakLevel(0.0f),
    m_peakLevelOut(0.0f),
    m_levelSum(0.0f),
    m_rmsLevel(0.0f),
    m_messageQueueToGUI(nullptr),
    m_interpolatorDistanceRemain(0.0f),
    m_interpolatorConsumed(false)
{
    m_specSampleBuffer.resize(m_specSampleBufferSize);
    m_interpolatorDistance = (Real)m_channelSampleRate / (Real)m_spectrumRate;
    m_interpolator.create(48, m_spectrumRate, m_spectrumRate / 2.2, 3.0);

    updateSamplesPerUnit();
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

    // Up-convert to carrier frequency
    Complex ci = m_modSample;
    ci *= m_carrierNco.nextIQ();

    // Power measurement
    double magsq = ci.real() * ci.real() + ci.imag() * ci.imag();
    m_movingAverage(magsq);
    m_magsq = m_movingAverage.asDouble();

    sample.m_real = (FixReal)(ci.real() * SDR_TX_SCALEF);
    sample.m_imag = (FixReal)(ci.imag() * SDR_TX_SCALEF);
}

void CWModSource::modulateSample()
{
    // Consume one sample from the current Morse element, advancing to the next when done
    if (m_unitSampleCount == 0)
    {
        if (!m_morseQueue.isEmpty())
        {
            const MorseElement& el = m_morseQueue.front();
            m_keyDown = el.key;
            m_unitSampleCount = el.units * m_samplesPerUnit;
            m_morseQueue.removeFirst();

            // Compute envelope ramp length: 5 ms at current sample rate, capped to half a unit
            int rampSamples = std::min(m_channelSampleRate / 200, m_samplesPerUnit / 2);
            m_envelopeStep = (rampSamples > 0) ? (1.0f / rampSamples) : 1.0f;
        }
        else
        {
            // No more elements: check repeat or stay silent
            m_repeatsDone++;
            bool shouldRepeat = m_settings.m_repeat &&
                (m_settings.m_repeatCount < 0 || m_repeatsDone < m_settings.m_repeatCount);

            if (shouldRepeat) {
                encodeText(m_settings.m_text);
            } else {
                m_keyDown = false;
            }
        }
    }

    if (m_unitSampleCount > 0) {
        m_unitSampleCount--;
    }

    // Smooth envelope: ramp up when key down, ramp down when key up
    if (m_keyDown) {
        m_envelope = std::min(1.0f, m_envelope + m_envelopeStep);
    } else {
        m_envelope = std::max(0.0f, m_envelope - m_envelopeStep);
    }

    Real amplitude = m_linearGain * m_envelope;
    m_modSample.real(amplitude);
    m_modSample.imag(0.0f);

    calculateLevel(amplitude);
    sampleToSpectrum(m_modSample);
}

void CWModSource::calculateLevel(Real& sample)
{
    Real absSample = std::fabs(sample);

    if (absSample > m_peakLevel) {
        m_peakLevel = absSample;
    }

    m_levelSum += absSample * absSample;
    m_levelCalcCount++;

    if (m_levelCalcCount >= (quint32)m_levelNbSamples)
    {
        m_rmsLevel = std::sqrt(m_levelSum / m_levelNbSamples);
        m_peakLevelOut = m_peakLevel;
        m_peakLevel = 0.0f;
        m_levelSum = 0.0f;
        m_levelCalcCount = 0;
    }
}

void CWModSource::sampleToSpectrum(Complex sample)
{
    if (!m_spectrumSink) {
        return;
    }

    if (m_interpolatorConsumed)
    {
        m_specSampleBuffer[m_specSampleBufferIndex++] = Sample(
            (FixReal)(sample.real() * SDR_TX_SCALEF),
            (FixReal)(sample.imag() * SDR_TX_SCALEF)
        );

        if (m_specSampleBufferIndex == m_specSampleBufferSize)
        {
            m_spectrumSink->feed(m_specSampleBuffer.begin(), m_specSampleBuffer.end(), false);
            m_specSampleBufferIndex = 0;
        }

        m_interpolatorConsumed = false;
    }

    Real interpolated;
    m_interpolatorDistanceRemain += m_interpolatorDistance;

    if (m_interpolatorDistanceRemain >= 1.0f)
    {
        m_interpolatorDistanceRemain -= 1.0f;
        m_interpolatorConsumed = m_interpolator.decimate(&m_interpolatorDistanceRemain, sample, &interpolated);
    }
}

void CWModSource::addTXText(const QString& text)
{
    m_repeatsDone = 0;
    encodeText(text);
}

void CWModSource::encodeText(const QString& text)
{
    bool firstChar = true;

    for (QChar qc : text)
    {
        char c = qc.toLatin1();

        if (c == ' ')
        {
            // Word space: 7 units total; inter-character already added 3 units after previous char,
            // so add 4 more (= 7 - 3)
            if (!firstChar) {
                m_morseQueue.append({ false, 4 });
            }
        }
        else
        {
            // Inter-character gap (3 units silence) between consecutive characters
            if (!firstChar) {
                m_morseQueue.append({ false, 3 });
            }

            buildMorseSequence(c, m_morseQueue);
            firstChar = false;
        }
    }
}

void CWModSource::buildMorseSequence(char c, QList<MorseElement>& out)
{
    const char *code = nullptr;
    c = std::toupper(c);

    if (c >= 'A' && c <= 'Z') {
        code = m_morseTable[c - 'A'];
    } else if (c >= '0' && c <= '9') {
        code = m_morseTable[26 + (c - '0')];
    }

    if (!code) {
        return; // Unknown character: skip
    }

    bool firstElement = true;

    for (const char *p = code; *p != '\0'; p++)
    {
        // Inter-element gap (1 unit silence) between consecutive elements within a character
        if (!firstElement) {
            out.append({ false, 1 });
        }

        if (*p == '.') {
            out.append({ true, 1 }); // Dot: 1 unit on
        } else if (*p == '-') {
            out.append({ true, 3 }); // Dash: 3 units on
        }

        firstElement = false;
    }
}

void CWModSource::updateSamplesPerUnit()
{
    // PARIS standard: one word = 50 Morse time units
    // dot_duration_ms = 1200 / WPM
    // samples_per_unit = sample_rate * dot_duration_ms / 1000
    float dotMs = 1200.0f / m_settings.m_wpm;
    m_samplesPerUnit = std::max(1, (int)(m_channelSampleRate * dotMs / 1000.0f));
}

void CWModSource::applySettings(const QStringList& settingsKeys, const CWModSettings& settings, bool force)
{
    if (settingsKeys.contains("gain") || force) {
        m_linearGain = std::pow(10.0f, settings.m_gain / 20.0f);
    }

    if (settingsKeys.contains("toneFrequency") || force) {
        m_carrierNco.setFreq(settings.m_toneFrequency, m_channelSampleRate);
    }

    if (settingsKeys.contains("wpm") || force) {
        m_settings.m_wpm = settings.m_wpm;
        updateSamplesPerUnit();
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

void CWModSource::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    (void) channelFrequencyOffset;

    if (channelSampleRate != m_channelSampleRate || force)
    {
        m_channelSampleRate = channelSampleRate;
        m_interpolatorDistance = (Real)m_channelSampleRate / (Real)m_spectrumRate;
        m_interpolator.create(48, m_spectrumRate, m_spectrumRate / 2.2, 3.0);
        m_carrierNco.setFreq(m_settings.m_toneFrequency, channelSampleRate);
        updateSamplesPerUnit();
    }

    m_channelFrequencyOffset = channelFrequencyOffset;
}
