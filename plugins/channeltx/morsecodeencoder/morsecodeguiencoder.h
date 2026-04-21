///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 SDRDemon Project                                           //
// Morse Code Encoder — GUI header                                               //
// GPLv3 or later                                                                //
///////////////////////////////////////////////////////////////////////////////////

#ifndef PLUGINS_CHANNELTX_MORSECODEENCODER_MORSECODEGUIENCODER_H
#define PLUGINS_CHANNELTX_MORSECODEENCODER_MORSECODEGUIENCODER_H

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "util/movingaverage.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "morsecodeencoder.h"
#include "morsecodencodersettings.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSource;
class SpectrumVis;

namespace Ui { class MorseCodeEncoderGUI; }

class MorseCodeEncoderGUI : public ChannelGUI {
    Q_OBJECT
public:
    static MorseCodeEncoderGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx);
    virtual void destroy();

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual void setWorkspaceIndex(int index) { m_settings.m_workspaceIndex = index; }
    virtual int getWorkspaceIndex() const { return m_settings.m_workspaceIndex; }
    virtual void setGeometryBytes(const QByteArray& blob) { m_settings.m_geometryBytes = blob; }
    virtual QByteArray getGeometryBytes() const { return m_settings.m_geometryBytes; }
    virtual QString getTitle() const { return m_settings.m_title; }
    virtual QColor getTitleColor() const { return m_settings.m_rgbColor; }
    virtual void zetHidden(bool hidden) { m_settings.m_hidden = hidden; }
    virtual bool getHidden() const { return m_settings.m_hidden; }
    virtual ChannelMarker& getChannelMarker() { return m_channelMarker; }
    virtual int getStreamIndex() const { return m_settings.m_streamIndex; }
    virtual void setStreamIndex(int streamIndex) { m_settings.m_streamIndex = streamIndex; }

public slots:
    void channelMarkerChangedByCursor();

private:
    Ui::MorseCodeEncoderGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    MorseCodeEncoderSettings m_settings;
    qint64 m_deviceCenterFrequency;
    int m_basebandSampleRate;
    bool m_doApplySettings;
    SpectrumVis* m_spectrumVis;
    MorseCodeEncoder* m_morseCodeEncoder;
    MovingAverageUtil<double, double, 20> m_channelPowerDbAvg;
    MessageQueue m_inputMessageQueue;

    explicit MorseCodeEncoderGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent = nullptr);
    virtual ~MorseCodeEncoderGUI();

    void blockApplySettings(bool block);
    void applySettings(const QStringList& settingKeys, bool force = false);
    void displaySettings();
    bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();

    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);

private slots:
    void handleSourceMessages();
    void on_deltaFrequency_changed(qint64 value);
    void on_toneFrequency_valueChanged(int value);
    void on_wpm_valueChanged(int value);
    void on_gain_valueChanged(int value);
    void on_channelMute_toggled(bool checked);
    void on_loop_toggled(bool checked);
    void on_txButton_clicked();
    void on_text_editingFinished();
    void on_morsePreview_clicked(bool checked);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void tick();
};

#endif // PLUGINS_CHANNELTX_MORSECODEENCODER_MORSECODEGUIENCODER_H
