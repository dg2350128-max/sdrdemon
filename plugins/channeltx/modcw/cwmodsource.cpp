///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 SDRDemon Project                                           //
//                                                                               //
// CW Modulator — sample source implementation                                   //
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

#include <QDebug>

#include "dsp/basebandsamplesink.h"
#include "dsp/datafifo.h"
#include "maincore.h"
#include "util/messagequeue.h"

#include "cwmod.h"
#include "cwmodsource.h"

CWModSource::CWModSource() :
    m_channelSampleRate(48000),
    m_channelFrequencyOffset(0),
    m_spectrumRate(2000),
    m_spectrumSink(nullptr),
    m_specSampleBufferIndex(0),
    m_magsq(0.0),
    m_levelCalcCount(0),
    m_peakLevel(0.0f),
    m_levelSum(0.0f),
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

CWModSource::~CWModSource()
{
}

void CWModSource::pull(SampleVector::iterator begin, unsigned int nbSamples)
{
    std::for_each(
        begin,
        begin + nbSamples,
        [this](Sample& s) { pullOne(s); }
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

    Real modSample;
    modulateSample(modSample);

    // Mix up to carrier frequency
    Complex carrier = m_carrierNco.nextIQ();
    Complex ci(modSample * carrier.real(), modSample * carrier.imag());

    double magsq = ci.real() * ci.real() + ci.imag() * ci.imag();
    m_movingAverage(magsq);
    m_magsq = m_movingAverage.asDouble();

    sampleToSpectrum(ci);

    Real s = std::abs(modSample);
    calculateLevel(s);

    sample.m_real = (FixReal)(ci.real() * SDR_TX_SCALEF);
    sample.m_imag = (FixReal)(ci.imag() * SDR_TX_SCALEF);
}

void CWModSource::modulateSample(Real& modSample)
{
    // Get the current key state from the CW keyer (1 = key down, 0 = key up)
    int cwSample = m_cwKeyer.getSample();

    float fadeSample;
    if (m_settings.m_useRiseTime && m_cwSmoother.getFadeSample(cwSample != 0, fadeSample)) {
        modSample = m_linearGain * fadeSample;
    } else {
        modSample = m_linearGain * (cwSample != 0 ? 1.0f : 0.0f);
    }

    // Write to demod analyser buffer
    m_demodBuffer[m_demodBufferFill] = (qint16)(modSample * std::numeric_limits<int16_t>::max());
    ++m_demodBufferFill;

    if (m_demodBufferFill >= m_demodBuffer.size())
    {
        QList<ObjectPipe*> dataPipes;
        MainCore::instance()->getDataPipes().getDataPipes(m_channel, "demod", dataPipes);

        for (auto& pipe : dataPipes)
        {
            DataFifo *fifo = qobject_cast<DataFifo*>(pipe->m_element);
            if (fifo) {
                fifo->write((quint8*)&m_demodBuffer[0], m_demodBuffer.size() * sizeof(qint16), DataFifo::DataTypeI16);
            }
        }

        m_demodBufferFill = 0;
    }
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
    if ((settingsKeys.contains("toneFrequency") && (settings.m_toneFrequency != m_settings.m_toneFrequency)) || force)
    {
        m_carrierNco.setFreq(settings.m_toneFrequency, m_channelSampleRate);
    }

    if ((settingsKeys.contains("gain") && (settings.m_gain != m_settings.m_gain)) || force)
    {
        m_linearGain = powf(10.0f, settings.m_gain / 20.0f);
    }

    if ((settingsKeys.contains("wpm") && (settings.m_wpm != m_settings.m_wpm)) || force)
    {
        CWKeyerSettings cwSettings = m_cwKeyer.getSettings();
        cwSettings.m_wpm = settings.m_wpm;
        cwSettings.m_sampleRate = m_channelSampleRate;
        m_cwKeyer.getInputMessageQueue()->push(CWKeyer::MsgConfigureCWKeyer::create(cwSettings, true));
    }

    if ((settingsKeys.contains("loop") && (settings.m_loop != m_settings.m_loop)) || force)
    {
        CWKeyerSettings cwSettings = m_cwKeyer.getSettings();
        cwSettings.m_loop = settings.m_loop;
        m_cwKeyer.getInputMessageQueue()->push(CWKeyer::MsgConfigureCWKeyer::create(cwSettings, false));
    }

    if ((settingsKeys.contains("text") && (settings.m_text != m_settings.m_text)) || force)
    {
        CWKeyerSettings cwSettings = m_cwKeyer.getSettings();
        cwSettings.m_text = settings.m_text;
        cwSettings.m_mode = CWKeyerSettings::CWText;
        m_cwKeyer.getInputMessageQueue()->push(CWKeyer::MsgConfigureCWKeyer::create(cwSettings, false));
        m_cwKeyer.resetText();
    }

    if ((settingsKeys.contains("useRiseTime") || settingsKeys.contains("riseTime")) || force)
    {
        // Rise/fall time in samples
        unsigned int nbFadeSamples = (unsigned int)(settings.m_riseTime * m_channelSampleRate / 1000.0f);
        if (settings.m_useRiseTime) {
            m_cwSmoother.setNbFadeSamples(nbFadeSamples > 0 ? nbFadeSamples : 1);
        }
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

void CWModSource::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "CWModSource::applyChannelSettings:"
             << " channelSampleRate: " << channelSampleRate
             << " channelFrequencyOffset: " << channelFrequencyOffset;

    if ((channelFrequencyOffset != m_channelFrequencyOffset)
     || (channelSampleRate != m_channelSampleRate) || force)
    {
        m_carrierNco.setFreq(m_settings.m_toneFrequency, channelSampleRate);
    }

    if ((m_channelSampleRate != channelSampleRate) || force)
    {
        m_interpolatorDistanceRemain = 0;
        m_interpolatorConsumed = false;
        m_interpolatorDistance = (Real)channelSampleRate / (Real)m_spectrumRate;
        m_interpolator.create(48, m_spectrumRate, m_spectrumRate / 2.2, 3.0);

        // Update CW keyer sample rate
        CWKeyerSettings cwSettings = m_cwKeyer.getSettings();
        cwSettings.m_sampleRate = channelSampleRate;
        m_cwKeyer.getInputMessageQueue()->push(CWKeyer::MsgConfigureCWKeyer::create(cwSettings, true));
        m_cwKeyer.setSampleRate(channelSampleRate);

        // Update fade samples
        unsigned int nbFadeSamples = (unsigned int)(m_settings.m_riseTime * channelSampleRate / 1000.0f);
        if (m_settings.m_useRiseTime) {
            m_cwSmoother.setNbFadeSamples(nbFadeSamples > 0 ? nbFadeSamples : 1);
        }
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_channel, "reportdemod", pipes);

    for (const auto& pipe : pipes)
    {
        MessageQueue* messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        MainCore::MsgChannelDemodReport *msg = MainCore::MsgChannelDemodReport::create(m_channel, m_channelSampleRate);
        messageQueue->push(msg);
    }
}
