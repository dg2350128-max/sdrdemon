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

#include <QDockWidget>
#include <QMainWindow>
#include <QDebug>

#include "dsp/spectrumvis.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/deviceuiset.h"
#include "plugin/pluginapi.h"
#include "util/db.h"
#include "util/morse.h"
#include "gui/crightclickenabler.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "maincore.h"

#include "ui_cwmodgui.h"
#include "cwmodgui.h"
#include "cwmodsource.h"

CWModGUI* CWModGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx)
{
    CWModGUI* gui = new CWModGUI(pluginAPI, deviceUISet, channelTx);
    return gui;
}

void CWModGUI::destroy()
{
    delete this;
}

void CWModGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(QStringList(), true);
}

QByteArray CWModGUI::serialize() const
{
    return m_settings.serialize();
}

bool CWModGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        displaySettings();
        applySettings(QStringList(), true);
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

bool CWModGUI::handleMessage(const Message& message)
{
    if (CWMod::MsgConfigureCWMod::match(message))
    {
        const CWMod::MsgConfigureCWMod& cfg = (CWMod::MsgConfigureCWMod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (CWMod::MsgReportTx::match(message))
    {
        // Transmission complete: restore TX button style
        ui->txButton->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        const DSPSignalNotification& notif = (const DSPSignalNotification&) message;
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = notif.getSampleRate();
        ui->deltaFrequency->setValueRange(false, 7, -m_basebandSampleRate / 2, m_basebandSampleRate / 2);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate / 2));
        updateAbsoluteCenterFrequency();
        return true;
    }
    else
    {
        return false;
    }
}

void CWModGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings(QStringList("inputFrequencyOffset"));
}

void CWModGUI::handleSourceMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void CWModGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings(QStringList("inputFrequencyOffset"));
}

void CWModGUI::on_wpm_valueChanged(int value)
{
    ui->wpmText->setText(QString("%1").arg(value));
    m_settings.m_wpm = value;
    applySettings(QStringList("wpm"));
}

void CWModGUI::on_gain_valueChanged(int value)
{
    ui->gainText->setText(QString("%1dB").arg(value));
    m_settings.m_gain = value;
    applySettings(QStringList("gain"));
}

void CWModGUI::on_channelMute_toggled(bool checked)
{
    m_settings.m_channelMute = checked;
    applySettings(QStringList("channelMute"));
}

void CWModGUI::on_text_editingFinished()
{
    m_settings.m_text = ui->text->text();
    updateMorseDisplay();
    applySettings(QStringList("text"));
}

void CWModGUI::on_txButton_clicked()
{
    if (m_settings.m_text.isEmpty()) {
        return;
    }
    CWMod::MsgTx* msg = CWMod::MsgTx::create();
    m_cwMod->getInputMessageQueue()->push(msg);
    ui->txButton->setStyleSheet("QToolButton { background-color : green; }");
}

void CWModGUI::on_repeat_toggled(bool checked)
{
    m_settings.m_repeat = checked;
    applySettings(QStringList("repeat"));
}

void CWModGUI::on_repeatCount_valueChanged(int value)
{
    m_settings.m_repeatCount = value;
    applySettings(QStringList("repeatCount"));
}

void CWModGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings(QStringList());
}

void CWModGUI::onMenuDialogCalled(const QPoint& p)
{
    if (m_contextMenuType == ContextMenuType::ContextMenuChannelSettings)
    {
        BasicChannelSettingsDialog dialog(&m_channelMarker, this);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);
        dialog.setReverseAPIChannelIndex(m_settings.m_reverseAPIChannelIndex);
        dialog.setDefaultTitle(m_displayedName);

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            dialog.setNumberOfStreams(m_cwMod->getNumberOfDeviceStreams());
            dialog.setStreamIndex(m_settings.m_streamIndex);
        }

        dialog.move(p);
        new DialogPositioner(&dialog, false);
        dialog.exec();

        m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
        m_settings.m_title = m_channelMarker.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
        m_settings.m_reverseAPIChannelIndex = dialog.getReverseAPIChannelIndex();

        setWindowTitle(m_settings.m_title);
        setTitle(m_channelMarker.getTitle());
        setTitleColor(m_settings.m_rgbColor);

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
            m_channelMarker.clearStreamIndexes();
            m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
            updateIndexLabel();
        }

        applySettings(QStringList({"rgbColor", "title", "useReverseAPI", "reverseAPIAddress",
                                    "reverseAPIPort", "reverseAPIDeviceIndex", "reverseAPIChannelIndex", "streamIndex"}));
    }

    resetContextMenuType();
}

CWModGUI::CWModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::CWModGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
    m_doApplySettings(true)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channeltx/modcw/readme.md";
    RollupContents *rollupContents = getRollupContents();
    ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
    connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_cwMod = (CWMod*) channelTx;
    m_cwMod->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    m_spectrumVis = m_cwMod->getSpectrumVis();
    m_spectrumVis->setGLSpectrum(ui->glSpectrum);

    ui->spectrumGUI->setBuddies(m_spectrumVis, ui->glSpectrum);

    ui->glSpectrum->setCenterFrequency(0);
    ui->glSpectrum->setSampleRate(8000);
    ui->glSpectrum->setLsbDisplay(false);

    SpectrumSettings spectrumSettings = m_spectrumVis->getSettings();
    spectrumSettings.m_ssb = false;
    spectrumSettings.m_displayCurrent = true;
    spectrumSettings.m_displayWaterfall = false;
    spectrumSettings.m_displayMaxHold = false;
    spectrumSettings.m_displayHistogram = false;
    SpectrumVis::MsgConfigureSpectrumVis *specMsg = SpectrumVis::MsgConfigureSpectrumVis::create(spectrumSettings, false);
    m_spectrumVis->getInputMessageQueue()->push(specMsg);

    ui->spectrumContainer->setVisible(false);

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::green);
    m_channelMarker.setBandwidth(500);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("CW Modulator");
    m_channelMarker.setSourceOrSinkStream(false);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));

    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
    m_cwMod->setLevelMeter(ui->volumeMeter);

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

    displaySettings();
    makeUIConnections();
    applySettings(QStringList(), true);
    DialPopup::addPopupsToChildDials(this);
    m_resizer.enableChildMouseTracking();
}

CWModGUI::~CWModGUI()
{
    delete ui;
}

void CWModGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void CWModGUI::applySettings(const QStringList& settingKeys, bool force)
{
    if (m_doApplySettings)
    {
        CWMod::MsgConfigureCWMod *msg = CWMod::MsgConfigureCWMod::create(settingKeys, m_settings, force);
        m_cwMod->getInputMessageQueue()->push(msg);
    }
}

void CWModGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor);

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    ui->wpm->setValue(m_settings.m_wpm);
    ui->wpmText->setText(QString("%1").arg(m_settings.m_wpm));
    ui->gain->setValue((int) m_settings.m_gain);
    ui->gainText->setText(QString("%1dB").arg((int) m_settings.m_gain));
    ui->channelMute->setChecked(m_settings.m_channelMute);
    ui->text->setText(m_settings.m_text);
    ui->repeat->setChecked(m_settings.m_repeat);
    ui->repeatCount->setValue(m_settings.m_repeatCount);

    updateMorseDisplay();

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();

    blockApplySettings(false);
}

void CWModGUI::updateMorseDisplay()
{
    ui->morseDisplay->setText(Morse::toUnicodeMorse(m_settings.m_text.toUpper()));
}

void CWModGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &CWModGUI::on_deltaFrequency_changed);
    QObject::connect(ui->wpm, QOverload<int>::of(&QSpinBox::valueChanged), this, &CWModGUI::on_wpm_valueChanged);
    QObject::connect(ui->gain, QOverload<int>::of(&QSlider::valueChanged), this, &CWModGUI::on_gain_valueChanged);
    QObject::connect(ui->channelMute, &QToolButton::toggled, this, &CWModGUI::on_channelMute_toggled);
    QObject::connect(ui->text, &QLineEdit::editingFinished, this, &CWModGUI::on_text_editingFinished);
    QObject::connect(ui->txButton, &QToolButton::clicked, this, &CWModGUI::on_txButton_clicked);
    QObject::connect(ui->repeat, &QToolButton::toggled, this, &CWModGUI::on_repeat_toggled);
    QObject::connect(ui->repeatCount, QOverload<int>::of(&QSpinBox::valueChanged), this, &CWModGUI::on_repeatCount_valueChanged);
}

void CWModGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}

void CWModGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void CWModGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void CWModGUI::tick()
{
    double powDb = CalcDb::dbPower(m_cwMod->getMagSq());
    m_channelPowerDbAvg(powDb);
    ui->channelPower->setText(tr("%1 dB").arg(m_channelPowerDbAvg.asDouble(), 0, 'f', 1));
}
