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

#include <QDockWidget>
#include <QMainWindow>
#include <QScrollBar>
#include <QDebug>

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/deviceuiset.h"
#include "plugin/pluginapi.h"
#include "util/db.h"
#include "gui/crightclickenabler.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "maincore.h"

#include "ui_morsemodgui.h"
#include "morsemodgui.h"

MorseModGUI* MorseModGUI::create(PluginAPI *pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx)
{
    MorseModGUI *gui = new MorseModGUI(pluginAPI, deviceUISet, channelTx);
    return gui;
}

void MorseModGUI::destroy()
{
    delete this;
}

void MorseModGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(QStringList(), true);
}

QByteArray MorseModGUI::serialize() const
{
    return m_settings.serialize();
}

bool MorseModGUI::deserialize(const QByteArray& data)
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

bool MorseModGUI::handleMessage(const Message& message)
{
    if (MorseMod::MsgConfigureMorseMod::match(message))
    {
        const MorseMod::MsgConfigureMorseMod& cfg = (MorseMod::MsgConfigureMorseMod&)message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (MorseMod::MsgReportTx::match(message))
    {
        const MorseMod::MsgReportTx& report = (MorseMod::MsgReportTx&)message;
        const QString& s = report.getText();
        int bufferedCharacters = report.getBufferedCharacters();

        // Show green TX button while transmitting
        if (bufferedCharacters == 0)
        {
            ui->txButton->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
        }
        else
        {
            ui->txButton->setStyleSheet("QToolButton { background-color : green; }");
        }

        if (!s.isEmpty())
        {
            int scrollPos = ui->transmittedText->verticalScrollBar()->value();
            bool atBottom = scrollPos >= ui->transmittedText->verticalScrollBar()->maximum();
            ui->transmittedText->moveCursor(QTextCursor::End);
            ui->transmittedText->verticalScrollBar()->setValue(scrollPos);
            ui->transmittedText->insertPlainText(s);
            if (atBottom) {
                ui->transmittedText->verticalScrollBar()->setValue(ui->transmittedText->verticalScrollBar()->maximum());
            }
        }
        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        const DSPSignalNotification& notif = (const DSPSignalNotification&)message;
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

void MorseModGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings(QStringList("inputFrequencyOffset"));
}

void MorseModGUI::handleSourceMessages()
{
    Message *message;
    while ((message = getInputMessageQueue()->pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void MorseModGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings(QStringList("inputFrequencyOffset"));
}

void MorseModGUI::on_toneFrequency_valueChanged(int value)
{
    m_settings.m_toneFrequency = value;
    applySettings(QStringList("toneFrequency"));
}

void MorseModGUI::on_wpm_valueChanged(int value)
{
    m_settings.m_wpm = value;
    applySettings(QStringList("wpm"));
}

void MorseModGUI::on_gain_valueChanged(int value)
{
    ui->gainText->setText(QString("%1dB").arg(value));
    m_settings.m_gain = value;
    applySettings(QStringList("gain"));
}

void MorseModGUI::on_channelMute_toggled(bool checked)
{
    m_settings.m_channelMute = checked;
    applySettings(QStringList("channelMute"));
}

void MorseModGUI::on_txButton_clicked()
{
    transmit(ui->text->text());
}

void MorseModGUI::on_text_returnPressed()
{
    transmit(ui->text->text());
    ui->text->clear();
}

void MorseModGUI::on_text_editingFinished()
{
    m_settings.m_text = ui->text->text();
    applySettings(QStringList("text"));
}

void MorseModGUI::on_repeat_toggled(bool checked)
{
    m_settings.m_repeat = checked;
    applySettings(QStringList("repeat"));
}

void MorseModGUI::on_udpEnabled_clicked(bool checked)
{
    m_settings.m_udpEnabled = checked;
    applySettings(QStringList("udpEnabled"));
}

void MorseModGUI::on_udpAddress_editingFinished()
{
    m_settings.m_udpAddress = ui->udpAddress->text();
    applySettings(QStringList("udpAddress"));
}

void MorseModGUI::on_udpPort_editingFinished()
{
    m_settings.m_udpPort = ui->udpPort->text().toInt();
    applySettings(QStringList("udpPort"));
}

void MorseModGUI::onWidgetRolled(QWidget *widget, bool rollDown)
{
    (void)widget;
    (void)rollDown;
    getRollupContents()->saveState(m_rollupState);
    applySettings(QStringList());
}

void MorseModGUI::onMenuDialogCalled(const QPoint& p)
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
            dialog.setNumberOfStreams(m_morseMod->getNumberOfDeviceStreams());
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

        applySettings(QStringList({
            "rgbColor", "title", "useReverseAPI", "reverseAPIAddress",
            "reverseAPIPort", "reverseAPIDeviceIndex", "reverseAPIChannelIndex", "streamIndex"
        }));
    }

    resetContextMenuType();
}

MorseModGUI::MorseModGUI(PluginAPI *pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget *parent) :
    ChannelGUI(parent),
    ui(new Ui::MorseModGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
    m_doApplySettings(true)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channeltx/modmorse/readme.md";
    RollupContents *rollupContents = getRollupContents();
    ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
    connect(rollupContents, SIGNAL(widgetRolled(QWidget*, bool)), this, SLOT(onWidgetRolled(QWidget*, bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(onMenuDialogCalled(const QPoint&)));

    m_morseMod = (MorseMod*)channelTx;
    m_morseMod->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::green);
    m_channelMarker.setBandwidth(2500);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("Morse (CW) Modulator");
    m_channelMarker.setSourceOrSinkStream(false);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));

    m_morseMod->setLevelMeter(ui->volumeMeter);

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

    displaySettings();
    makeUIConnections();
    applySettings(QStringList(), true);
    DialPopup::addPopupsToChildDials(this);
    m_resizer.enableChildMouseTracking();

    m_initialToolTip = ui->txButton->toolTip();
}

MorseModGUI::~MorseModGUI()
{
    delete ui;
}

void MorseModGUI::transmit(const QString& text)
{
    MorseMod::MsgTXText *msg = MorseMod::MsgTXText::create(text);
    m_morseMod->getInputMessageQueue()->push(msg);
}

void MorseModGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void MorseModGUI::applySettings(const QStringList& settingKeys, bool force)
{
    if (m_doApplySettings)
    {
        MorseMod::MsgConfigureMorseMod *msg = MorseMod::MsgConfigureMorseMod::create(settingKeys, m_settings, force);
        m_morseMod->getInputMessageQueue()->push(msg);
    }
}

void MorseModGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor);

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());
    updateIndexLabel();

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    ui->wpm->setValue(m_settings.m_wpm);
    ui->toneFrequency->setValue(m_settings.m_toneFrequency);
    ui->gain->setValue((int)m_settings.m_gain);
    ui->gainText->setText(QString("%1dB").arg((int)m_settings.m_gain));
    ui->channelMute->setChecked(m_settings.m_channelMute);
    ui->repeat->setChecked(m_settings.m_repeat);
    ui->text->setText(m_settings.m_text);
    ui->udpEnabled->setChecked(m_settings.m_udpEnabled);
    ui->udpAddress->setText(m_settings.m_udpAddress);
    ui->udpPort->setText(QString::number(m_settings.m_udpPort));

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();

    blockApplySettings(false);
}

void MorseModGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &MorseModGUI::on_deltaFrequency_changed);
    QObject::connect(ui->toneFrequency, QOverload<int>::of(&QSpinBox::valueChanged), this, &MorseModGUI::on_toneFrequency_valueChanged);
    QObject::connect(ui->wpm, QOverload<int>::of(&QSpinBox::valueChanged), this, &MorseModGUI::on_wpm_valueChanged);
    QObject::connect(ui->gain, &QSlider::valueChanged, this, &MorseModGUI::on_gain_valueChanged);
    QObject::connect(ui->channelMute, &QToolButton::toggled, this, &MorseModGUI::on_channelMute_toggled);
    QObject::connect(ui->txButton, &QToolButton::clicked, this, &MorseModGUI::on_txButton_clicked);
    QObject::connect(ui->text, &QLineEdit::returnPressed, this, &MorseModGUI::on_text_returnPressed);
    QObject::connect(ui->text, &QLineEdit::editingFinished, this, &MorseModGUI::on_text_editingFinished);
    QObject::connect(ui->repeat, &QToolButton::toggled, this, &MorseModGUI::on_repeat_toggled);
    QObject::connect(ui->udpEnabled, &QCheckBox::clicked, this, &MorseModGUI::on_udpEnabled_clicked);
    QObject::connect(ui->udpAddress, &QLineEdit::editingFinished, this, &MorseModGUI::on_udpAddress_editingFinished);
    QObject::connect(ui->udpPort, &QLineEdit::editingFinished, this, &MorseModGUI::on_udpPort_editingFinished);
}

void MorseModGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}

void MorseModGUI::leaveEvent(QEvent*)
{
    m_channelMarker.setHighlighted(false);
}

void MorseModGUI::enterEvent(EnterEventType*)
{
    m_channelMarker.setHighlighted(true);
}

void MorseModGUI::tick()
{
    double powDb = CalcDb::dbPower(m_morseMod->getMagSq());
    m_channelPowerDbAvg(powDb);
    ui->channelPower->setText(tr("%1 dB").arg(m_channelPowerDbAvg.asDouble(), 0, 'f', 1));
}
