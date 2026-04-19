///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 SDRDemon contributors                                     //
//                                                                               //
// Morse Encoder (CW) modulator plugin — OOK sample source implementation       //
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
#include <QDebug>

#include "dsp/basebandsamplesink.h"
#include "util/messagequeue.h"
#include "maincore.h"

#include "cwmod.h"
#include "cwmodsource.h"

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

CWModSource::CWModSource() :
    m_channelSampleRate(48000),
    m_channelFrequencyOffset(0),
    m_spectrumRate(2000),
    m_channel(nullptr),
    m_linearGain(1.0f),
    m_carrierOn(false),
    m_samplesRemainingInSymbol(0),
    m_serialKeyer(nullptr),
    m_keyerDown(0),
    m_spectrumSink(nullptr),
    m_specSampleBufferIndex(0),
    m_magsq(0.0),
    m_levelCalcCount(0),
    m_peakLevel(0.0f),
    m_levelSum(0.0f),
    m_messageQueueToGUI(nullptr)
{
    m_specSampleBuffer.resize(m_specSampleBufferSize);
    m_interpolatorDistanceRemain = 0;
    m_interpolatorConsumed = false;
    m_interpolatorDistance = static_cast<Real>(m_channelSampleRate) / static_cast<Real>(m_spectrumRate);
    m_interpolator.create(48, m_spectrumRate, m_spectrumRate / 2.2, 3.0);

    m_lowpass.create(301, m_channelSampleRate, 250.0);

    applySettings(QStringList(), m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

CWModSource::~CWModSource()
{
    closeKeyer();
}

// ---------------------------------------------------------------------------
// Pull
// ---------------------------------------------------------------------------

void CWModSource::pull(SampleVector::iterator begin, unsigned int nbSamples)
{
    std::for_each(begin, begin + nbSamples, [this](Sample& s) { pullOne(s); });
}

void CWModSource::pullOne(Sample& sample)
{
    if (m_settings.m_channelMute)
    {
        sample.m_real = 0;
        sample.m_imag = 0;
        return;
    }

    modulateSample();

    Complex ci = m_modSample;
    ci *= m_carrierNco.nextIQ();

    double magsq = ci.real() * ci.real() + ci.imag() * ci.imag();
    m_movingAverage(magsq);
    m_magsq = m_movingAverage.asDouble();

    sample.m_real = static_cast<FixReal>(ci.real() * SDR_TX_SCALEF);
    sample.m_imag = static_cast<FixReal>(ci.imag() * SDR_TX_SCALEF);
}

// ---------------------------------------------------------------------------
// Modulation
// ---------------------------------------------------------------------------

void CWModSource::modulateSample()
{
    // Determine whether the carrier should be on this sample.
    // Priority: USB keyer key-state overrides queued Morse symbols when keyer is enabled.
    bool on = false;

    if (m_settings.m_keyerEnabled)
    {
        on = (m_keyerDown.loadRelaxed() != 0);
    }
    else
    {
        // Advance through the symbol queue
        if (m_samplesRemainingInSymbol <= 0)
        {
            advanceSymbol();
        }

        if (m_samplesRemainingInSymbol > 0)
        {
            on = m_carrierOn;
            m_samplesRemainingInSymbol--;
        }
    }

    // OOK: multiply the NCO by gain when carrier is on, silence when off
    if (on)
    {
        m_modSample.real(m_linearGain);
        m_modSample.imag(0.0f);
    }
    else
    {
        m_modSample.real(0.0f);
        m_modSample.imag(0.0f);
    }

    m_modSample = m_lowpass.filter(m_modSample);

    sampleToSpectrum(m_modSample);

    Real s = std::real(m_modSample);
    calculateLevel(s);
}

// ---------------------------------------------------------------------------
// Symbol queue management
// ---------------------------------------------------------------------------

void CWModSource::advanceSymbol()
{
    if (!m_symbolQueue.isEmpty())
    {
        const MorseSymbol& sym = m_symbolQueue.takeFirst();
        m_carrierOn = sym.on;
        m_samplesRemainingInSymbol = sym.samples;
        return;
    }

    // Queue is empty — try to convert more text
    if (!m_textToTransmit.isEmpty())
    {
        // Take one character at a time so we report progress after each character
        QString ch = m_textToTransmit.left(1);
        m_textToTransmit = m_textToTransmit.mid(1);
        enqueueText(ch);

        if (!m_symbolQueue.isEmpty())
        {
            const MorseSymbol& sym = m_symbolQueue.takeFirst();
            m_carrierOn = sym.on;
            m_samplesRemainingInSymbol = sym.samples;
        }
    }
    else
    {
        // Nothing to send; stay silent with a short idle interval
        m_carrierOn = false;
        m_samplesRemainingInSymbol = m_channelSampleRate / 10; // 100 ms idle
    }
}

/**
 * Convert a text string to MorseSymbol entries and append them to m_symbolQueue.
 *
 * Timing (PARIS standard):
 *   unit = channelSampleRate * 1.2 / wpm  samples
 *   dot  = 1 unit on
 *   dash = 3 units on
 *   inter-element gap = 1 unit off
 *   inter-character gap = 3 units off total (1 already used, add 2 more)
 *   inter-word gap = 7 units off total (1 already used, add 6 more)
 */
void CWModSource::enqueueText(const QString& text)
{
    const int unit = static_cast<int>(std::round(
        static_cast<double>(m_channelSampleRate) * 1.2 / static_cast<double>(m_settings.m_wpm)));

    for (int ci = 0; ci < text.size(); ci++)
    {
        QChar c = text.at(ci);

        if (c == ' ')
        {
            // Inter-word gap: 7 units. We already placed 1-unit gap after the
            // previous character, so add 6 more.
            m_symbolQueue.append({false, 6 * unit});
            continue;
        }

        // Get the Morse sequence for this character (e.g. ".-." for 'R')
        QString morse = Morse::toMorse(c.toLatin1());

        for (int mi = 0; mi < morse.size(); mi++)
        {
            QChar sym = morse.at(mi);

            if (sym == '.')
            {
                m_symbolQueue.append({true, unit});           // dot on
            }
            else if (sym == '-')
            {
                m_symbolQueue.append({true, 3 * unit});       // dash on
            }
            else
            {
                continue; // skip unknown symbols
            }

            // Inter-element gap (1 unit off) after each dot or dash,
            // except the last element of the character
            if (mi < morse.size() - 1) {
                m_symbolQueue.append({false, unit});
            }
        }

        // Inter-character gap: 3 units total. After the last element we
        // already have 0 gap, so we add 3 units.  (Some implementations
        // place 1 unit after each element and then 2 more for inter-char,
        // but this simpler approach produces equivalent timing.)
        if (!morse.isEmpty()) {
            m_symbolQueue.append({false, 3 * unit});
        }
    }

    // Report to GUI
    if (m_messageQueueToGUI)
    {
        CWMod::MsgReportTx* msg = CWMod::MsgReportTx::create(text, m_textToTransmit.size());
        m_messageQueueToGUI->push(msg);
    }
}

void CWModSource::addTXText(const QString& text)
{
    int count = (m_settings.m_repeat && m_settings.m_repeatCount > 0)
        ? m_settings.m_repeatCount : 1;

    for (int i = 0; i < count; i++) {
        m_textToTransmit.append(text).append(' '); // add inter-word gap
    }
}

void CWModSource::setKeyerState(bool keyDown)
{
    m_keyerDown.storeRelaxed(keyDown ? 1 : 0);
}

// ---------------------------------------------------------------------------
// USB Serial keyer
// ---------------------------------------------------------------------------

void CWModSource::openKeyer(const QString& portName)
{
    closeKeyer();

    if (portName.isEmpty()) {
        return;
    }

    m_serialKeyer = new QSerialPort(portName);
    m_serialKeyer->setBaudRate(QSerialPort::Baud9600);
    m_serialKeyer->setDataBits(QSerialPort::Data8);
    m_serialKeyer->setParity(QSerialPort::NoParity);
    m_serialKeyer->setStopBits(QSerialPort::OneStop);
    m_serialKeyer->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serialKeyer->open(QIODevice::ReadOnly))
    {
        qWarning() << "CWModSource::openKeyer: failed to open" << portName
                   << m_serialKeyer->errorString();
        delete m_serialKeyer;
        m_serialKeyer = nullptr;
        return;
    }

    qDebug() << "CWModSource::openKeyer: opened" << portName;

    // Connect CTS changed signal for keyers that assert CTS on key-down
    QObject::connect(m_serialKeyer, &QSerialPort::pinoutSignalsChanged,
        [this](QSerialPort::PinoutSignals signals)
        {
            setKeyerState(signals.testFlag(QSerialPort::ClearToSendSignal));
        });

    // Also read bytes for keyers that send serial data
    QObject::connect(m_serialKeyer, &QSerialPort::readyRead,
        [this]()
        {
            QByteArray data = m_serialKeyer->readAll();
            if (!data.isEmpty()) {
                // Any non-zero byte = key down; zero byte = key up
                setKeyerState(data.back() != 0);
            }
        });
}

void CWModSource::closeKeyer()
{
    if (m_serialKeyer)
    {
        m_serialKeyer->close();
        m_serialKeyer->deleteLater();
        m_serialKeyer = nullptr;
        m_keyerDown.storeRelaxed(0);
    }
}

// ---------------------------------------------------------------------------
// applySettings / applyChannelSettings
// ---------------------------------------------------------------------------

void CWModSource::applySettings(const QStringList& settingsKeys, const CWModSettings& settings, bool force)
{
    if ((settingsKeys.contains("rfBandwidth") && (settings.m_rfBandwidth != m_settings.m_rfBandwidth)) || force)
    {
        m_lowpass.create(301, m_channelSampleRate, settings.m_rfBandwidth / 2.0);
    }

    if ((settingsKeys.contains("keyerEnabled") || settingsKeys.contains("keyerPort") || force))
    {
        if (settings.m_keyerEnabled && !settings.m_keyerPort.isEmpty())
        {
            if (force || settings.m_keyerPort != m_settings.m_keyerPort || settings.m_keyerEnabled != m_settings.m_keyerEnabled) {
                openKeyer(settings.m_keyerPort);
            }
        }
        else
        {
            closeKeyer();
        }
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
             << " channelSampleRate:" << channelSampleRate
             << " channelFrequencyOffset:" << channelFrequencyOffset;

    if ((channelFrequencyOffset != m_channelFrequencyOffset)
     || (channelSampleRate != m_channelSampleRate) || force)
    {
        m_carrierNco.setFreq(channelFrequencyOffset, channelSampleRate);
    }

    if ((m_channelSampleRate != channelSampleRate) || force)
    {
        m_lowpass.create(301, channelSampleRate, m_settings.m_rfBandwidth / 2.0);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorConsumed = false;
        m_interpolatorDistance = static_cast<Real>(channelSampleRate) / static_cast<Real>(m_spectrumRate);
        m_interpolator.create(48, m_spectrumRate, m_spectrumRate / 2.2, 3.0);
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;

    // Invalidate queued symbols so timing is recalculated at the new sample rate
    m_symbolQueue.clear();
    m_samplesRemainingInSymbol = 0;

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_channel, "reportdemod", pipes);

    for (const auto& pipe : pipes)
    {
        MessageQueue* messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        MainCore::MsgChannelDemodReport* msg = MainCore::MsgChannelDemodReport::create(
            m_channel, m_channelSampleRate);
        messageQueue->push(msg);
    }
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

void CWModSource::calculateLevel(Real& sample)
{
    if (m_levelCalcCount < static_cast<quint32>(m_levelNbSamples))
    {
        m_peakLevel = std::max(std::fabs(m_peakLevel), sample);
        m_levelSum += sample * sample;
        m_levelCalcCount++;
    }
    else
    {
        m_rmsLevel = std::sqrt(m_levelSum / m_levelNbSamples);
        m_peakLevelOut = m_peakLevel;
        m_peakLevel  = 0.0f;
        m_levelSum   = 0.0f;
        m_levelCalcCount = 0;
    }
}

void CWModSource::sampleToSpectrum(Complex sample)
{
    if (!m_spectrumSink) {
        return;
    }

    Complex out;
    if (m_interpolator.decimate(&m_interpolatorDistanceRemain, sample, &out))
    {
        m_interpolatorDistanceRemain += m_interpolatorDistance;
        Real r = std::real(out) * SDR_TX_SCALEF;
        Real i = std::imag(out) * SDR_TX_SCALEF;
        m_specSampleBuffer[m_specSampleBufferIndex++] = Sample(r, i);

        if (m_specSampleBufferIndex == m_specSampleBufferSize)
        {
            m_spectrumSink->feed(
                m_specSampleBuffer.begin(), m_specSampleBuffer.end(), false);
            m_specSampleBufferIndex = 0;
        }
    }
}
