#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

class UserSettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool enableDeviceManager READ enableDeviceManager WRITE setEnableDeviceManager NOTIFY enableDeviceManagerChanged)
    Q_PROPERTY(bool enableApplicationMixer READ enableApplicationMixer WRITE setEnableApplicationMixer NOTIFY enableApplicationMixerChanged)
    Q_PROPERTY(bool enableMediaSessionManager READ enableMediaSessionManager WRITE setEnableMediaSessionManager NOTIFY enableMediaSessionManagerChanged)
    Q_PROPERTY(int panelPosition READ panelPosition WRITE setPanelPosition NOTIFY panelPositionChanged)
    Q_PROPERTY(int taskbarOffset READ taskbarOffset WRITE setTaskbarOffset NOTIFY taskbarOffsetChanged)
    Q_PROPERTY(int xAxisMargin READ xAxisMargin WRITE setXAxisMargin NOTIFY xAxisMarginChanged)
    Q_PROPERTY(int yAxisMargin READ yAxisMargin WRITE setYAxisMargin NOTIFY yAxisMarginChanged)
    Q_PROPERTY(int languageIndex READ languageIndex WRITE setLanguageIndex NOTIFY languageIndexChanged)

    Q_PROPERTY(QVariantList commApps READ commApps WRITE setCommApps NOTIFY commAppsChanged)
    Q_PROPERTY(int chatMixValue READ chatMixValue WRITE setChatMixValue NOTIFY chatMixValueChanged)
    Q_PROPERTY(bool chatMixEnabled READ chatMixEnabled WRITE setChatMixEnabled NOTIFY chatMixEnabledChanged)
    Q_PROPERTY(bool activateChatmix READ activateChatmix WRITE setActivateChatmix NOTIFY activateChatmixChanged)
    Q_PROPERTY(bool showAudioLevel READ showAudioLevel WRITE setShowAudioLevel NOTIFY showAudioLevelChanged)
    Q_PROPERTY(int chatmixRestoreVolume READ chatmixRestoreVolume WRITE setChatmixRestoreVolume NOTIFY chatmixRestoreVolumeChanged)

    Q_PROPERTY(bool globalShortcutsEnabled READ globalShortcutsEnabled WRITE setGlobalShortcutsEnabled NOTIFY globalShortcutsEnabledChanged)
    Q_PROPERTY(int panelShortcutKey READ panelShortcutKey WRITE setPanelShortcutKey NOTIFY panelShortcutKeyChanged)
    Q_PROPERTY(int panelShortcutModifiers READ panelShortcutModifiers WRITE setPanelShortcutModifiers NOTIFY panelShortcutModifiersChanged)
    Q_PROPERTY(int chatMixShortcutKey READ chatMixShortcutKey WRITE setChatMixShortcutKey NOTIFY chatMixShortcutKeyChanged)
    Q_PROPERTY(int chatMixShortcutModifiers READ chatMixShortcutModifiers WRITE setChatMixShortcutModifiers NOTIFY chatMixShortcutModifiersChanged)
    Q_PROPERTY(bool chatMixShortcutNotification READ chatMixShortcutNotification WRITE setChatMixShortcutNotification NOTIFY chatMixShortcutNotificationChanged)
    Q_PROPERTY(int micMuteShortcutKey READ micMuteShortcutKey WRITE setMicMuteShortcutKey NOTIFY micMuteShortcutKeyChanged)
    Q_PROPERTY(int micMuteShortcutModifiers READ micMuteShortcutModifiers WRITE setMicMuteShortcutModifiers NOTIFY micMuteShortcutModifiersChanged)
    Q_PROPERTY(bool autoUpdateTranslations READ autoUpdateTranslations WRITE setAutoUpdateTranslations NOTIFY autoUpdateTranslationsChanged)
    Q_PROPERTY(bool firstRun READ firstRun WRITE setFirstRun NOTIFY firstRunChanged)

    Q_PROPERTY(int trayIconTheme READ trayIconTheme WRITE setTrayIconTheme NOTIFY trayIconThemeChanged)
    Q_PROPERTY(int iconStyle READ iconStyle WRITE setIconStyle NOTIFY iconStyleChanged)

    Q_PROPERTY(bool autoFetchForAppUpdates READ autoFetchForAppUpdates WRITE setAutoFetchForAppUpdates NOTIFY autoFetchForAppUpdatesChanged)
    Q_PROPERTY(bool headsetcontrolMonitoring READ headsetcontrolMonitoring WRITE setHeadsetcontrolMonitoring NOTIFY headsetcontrolMonitoringChanged)
    Q_PROPERTY(bool headsetcontrolLights READ headsetcontrolLights WRITE setHeadsetcontrolLights NOTIFY headsetcontrolLightsChanged)
    Q_PROPERTY(int headsetcontrolSidetone READ headsetcontrolSidetone WRITE setHeadsetcontrolSidetone NOTIFY headsetcontrolSidetoneChanged)
    Q_PROPERTY(bool allowBrightnessControl READ allowBrightnessControl WRITE setAllowBrightnessControl NOTIFY allowBrightnessControlChanged)
    Q_PROPERTY(bool avoidApplicationsOverflow READ avoidApplicationsOverflow WRITE setAvoidApplicationsOverflow NOTIFY avoidApplicationsOverflowChanged)
    Q_PROPERTY(int ddcciQueueDelay READ ddcciQueueDelay WRITE setDdcciQueueDelay NOTIFY ddcciQueueDelayChanged)

    Q_PROPERTY(bool enablePowerMenu READ enablePowerMenu WRITE setEnablePowerMenu NOTIFY enablePowerMenuChanged)
    Q_PROPERTY(bool showPowerDialogConfirmation READ showPowerDialogConfirmation WRITE setShowPowerDialogConfirmation NOTIFY showPowerDialogConfirmationChanged)

    Q_PROPERTY(int ddcciBrightness READ ddcciBrightness WRITE setDdcciBrightness NOTIFY ddcciBrightnessChanged)
    Q_PROPERTY(bool displayBatteryFooter READ displayBatteryFooter WRITE setDisplayBatteryFooter NOTIFY displayBatteryFooterChanged)
    Q_PROPERTY(int panelStyle READ panelStyle WRITE setPanelStyle NOTIFY panelStyleChanged)
    Q_PROPERTY(int headsetcontrolFetchRate READ headsetcontrolFetchRate WRITE setHeadsetcontrolFetchRate NOTIFY headsetcontrolFetchRateChanged)
    Q_PROPERTY(bool enableNotifications READ enableNotifications WRITE setEnableNotifications NOTIFY enableNotificationsChanged)

    Q_PROPERTY(bool enableMediaOverlay READ enableMediaOverlay WRITE setEnableMediaOverlay NOTIFY enableMediaOverlayChanged)
    Q_PROPERTY(int mediaOverlayPosition READ mediaOverlayPosition WRITE setMediaOverlayPosition NOTIFY mediaOverlayPositionChanged)
    Q_PROPERTY(int mediaOverlaySize READ mediaOverlaySize WRITE setMediaOverlaySize NOTIFY mediaOverlaySizeChanged)

    Q_PROPERTY(int sliderWheelSensivity READ sliderWheelSensivity WRITE setSliderWheelSensivity NOTIFY sliderWheelSensivityChanged)

    Q_PROPERTY(bool enableInternalBattery READ enableInternalBattery WRITE setEnableInternalBattery NOTIFY enableInternalBatteryChanged)

public:
    static UserSettings* create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);
    static UserSettings* instance();

    // Getters
    bool enableDeviceManager() const { return m_enableDeviceManager; }
    bool enableApplicationMixer() const { return m_enableApplicationMixer; }
    bool enableMediaSessionManager() const { return m_enableMediaSessionManager; }
    int panelPosition() const { return m_panelPosition; }
    int taskbarOffset() const { return m_taskbarOffset; }
    int xAxisMargin() const { return m_xAxisMargin; }
    int yAxisMargin() const { return m_yAxisMargin; }
    int languageIndex() const { return m_languageIndex; }

    QVariantList commApps() const { return m_commApps; }
    int chatMixValue() const { return m_chatMixValue; }
    bool chatMixEnabled() const { return m_chatMixEnabled; }
    bool activateChatmix() const { return m_activateChatmix; }
    bool showAudioLevel() const { return m_showAudioLevel; }
    int chatmixRestoreVolume() const { return m_chatmixRestoreVolume; }

    bool globalShortcutsEnabled() const { return m_globalShortcutsEnabled; }
    int panelShortcutKey() const { return m_panelShortcutKey; }
    int panelShortcutModifiers() const { return m_panelShortcutModifiers; }
    int chatMixShortcutKey() const { return m_chatMixShortcutKey; }
    int chatMixShortcutModifiers() const { return m_chatMixShortcutModifiers; }
    bool chatMixShortcutNotification() const { return m_chatMixShortcutNotification; }
    int micMuteShortcutKey() const { return m_micMuteShortcutKey; }
    int micMuteShortcutModifiers() const { return m_micMuteShortcutModifiers; }
    bool autoUpdateTranslations() const { return m_autoUpdateTranslations; }
    bool firstRun() const { return m_firstRun; }

    int trayIconTheme() const { return m_trayIconTheme; }
    int iconStyle() const { return m_iconStyle; }

    bool autoFetchForAppUpdates() const { return m_autoFetchForAppUpdates; }
    bool headsetcontrolMonitoring() const { return m_headsetcontrolMonitoring; }
    bool headsetcontrolLights() const { return m_headsetcontrolLights; }
    int headsetcontrolSidetone() const { return m_headsetcontrolSidetone; }
    bool allowBrightnessControl() const { return m_allowBrightnessControl; }
    bool avoidApplicationsOverflow() const { return m_avoidApplicationsOverflow; }
    int ddcciQueueDelay() const { return m_ddcciQueueDelay; }

    bool enablePowerMenu() const { return m_enablePowerMenu; }
    bool showPowerDialogConfirmation() const { return m_showPowerDialogConfirmation; }

    int ddcciBrightness() const { return m_ddcciBrightness; }
    bool displayBatteryFooter() const { return m_displayBatteryFooter; }
    int panelStyle() const { return m_panelStyle; }
    int headsetcontrolFetchRate() const { return m_headsetcontrolFetchRate; }
    bool enableNotifications() const { return m_enableNotifications; }

    bool enableMediaOverlay() const { return m_enableMediaOverlay; }
    int mediaOverlayPosition() const { return m_mediaOverlayPosition; }
    int mediaOverlaySize() const { return m_mediaOverlaySize; }

    int sliderWheelSensivity() const { return m_sliderWheelSensivity; }

    bool enableInternalBattery() const { return m_enableInternalBattery; }

    // Setters
    void setEnableDeviceManager(bool value);
    void setEnableApplicationMixer(bool value);
    void setEnableMediaSessionManager(bool value);
    void setPanelPosition(int value);
    void setTaskbarOffset(int value);
    void setXAxisMargin(int value);
    void setYAxisMargin(int value);
    void setLanguageIndex(int value);

    void setCommApps(const QVariantList &value);
    void setChatMixValue(int value);
    void setChatMixEnabled(bool value);
    void setActivateChatmix(bool value);
    void setShowAudioLevel(bool value);
    void setChatmixRestoreVolume(int value);

    void setGlobalShortcutsEnabled(bool value);
    void setPanelShortcutKey(int value);
    void setPanelShortcutModifiers(int value);
    void setChatMixShortcutKey(int value);
    void setChatMixShortcutModifiers(int value);
    void setChatMixShortcutNotification(bool value);
    void setMicMuteShortcutKey(int value);
    void setMicMuteShortcutModifiers(int value);
    void setAutoUpdateTranslations(bool value);
    void setFirstRun(bool value);

    void setTrayIconTheme(int value);
    void setIconStyle(int value);

    void setAutoFetchForAppUpdates(bool value);
    void setHeadsetcontrolMonitoring(bool value);
    void setHeadsetcontrolLights(bool value);
    void setHeadsetcontrolSidetone(int value);
    void setAllowBrightnessControl(bool value);
    void setAvoidApplicationsOverflow(bool value);
    void setDdcciQueueDelay(int value);

    void setEnablePowerMenu(bool value);
    void setShowPowerDialogConfirmation(bool value);

    void setDdcciBrightness(int value);
    void setDisplayBatteryFooter(bool value);
    void setPanelStyle(int value);
    void setHeadsetcontrolFetchRate(int value);
    void setEnableNotifications(bool value);

    void setEnableMediaOverlay(bool value);
    void setMediaOverlayPosition(int value);
    void setMediaOverlaySize(int value);

    void setSliderWheelSensivity(int value);

    void setEnableInternalBattery(bool value);

signals:
    void enableDeviceManagerChanged();
    void enableApplicationMixerChanged();
    void enableMediaSessionManagerChanged();
    void panelPositionChanged();
    void taskbarOffsetChanged();
    void xAxisMarginChanged();
    void yAxisMarginChanged();
    void languageIndexChanged();

    void commAppsChanged();
    void chatMixValueChanged();
    void chatMixEnabledChanged();
    void activateChatmixChanged();
    void showAudioLevelChanged();
    void chatmixRestoreVolumeChanged();

    void globalShortcutsEnabledChanged();
    void panelShortcutKeyChanged();
    void panelShortcutModifiersChanged();
    void chatMixShortcutKeyChanged();
    void chatMixShortcutModifiersChanged();
    void chatMixShortcutNotificationChanged();
    void micMuteShortcutKeyChanged();
    void micMuteShortcutModifiersChanged();
    void autoUpdateTranslationsChanged();
    void firstRunChanged();

    void trayIconThemeChanged();
    void iconStyleChanged();

    void autoFetchForAppUpdatesChanged();
    void headsetcontrolMonitoringChanged();
    void headsetcontrolLightsChanged();
    void headsetcontrolSidetoneChanged();
    void allowBrightnessControlChanged();
    void avoidApplicationsOverflowChanged();
    void ddcciQueueDelayChanged();

    void enablePowerMenuChanged();
    void showPowerDialogConfirmationChanged();

    void ddcciBrightnessChanged();
    void displayBatteryFooterChanged();
    void panelStyleChanged();
    void headsetcontrolFetchRateChanged();
    void enableNotificationsChanged();

    void enableMediaOverlayChanged();
    void mediaOverlayPositionChanged();
    void mediaOverlaySizeChanged();

    void sliderWheelSensivityChanged();

    void enableInternalBatteryChanged();

private:
    explicit UserSettings(QObject *parent = nullptr);
    static UserSettings* m_instance;

    void initProperties();
    void saveValue(const QString &key, const QVariant &value);

    bool m_enableDeviceManager;
    bool m_enableApplicationMixer;
    bool m_enableMediaSessionManager;
    int m_panelPosition;
    int m_taskbarOffset;
    int m_xAxisMargin;
    int m_yAxisMargin;
    int m_languageIndex;

    QVariantList m_commApps;
    int m_chatMixValue;
    bool m_chatMixEnabled;
    bool m_activateChatmix;
    bool m_showAudioLevel;
    int m_chatmixRestoreVolume;

    bool m_globalShortcutsEnabled;
    int m_panelShortcutKey;
    int m_panelShortcutModifiers;
    int m_chatMixShortcutKey;
    int m_chatMixShortcutModifiers;
    bool m_chatMixShortcutNotification;
    int m_micMuteShortcutKey;
    int m_micMuteShortcutModifiers;
    bool m_autoUpdateTranslations;
    bool m_firstRun;

    int m_trayIconTheme;
    int m_iconStyle;

    bool m_autoFetchForAppUpdates;
    bool m_headsetcontrolMonitoring;
    bool m_headsetcontrolLights;
    int m_headsetcontrolSidetone;
    bool m_allowBrightnessControl;
    bool m_avoidApplicationsOverflow;
    int m_ddcciQueueDelay;

    bool m_enablePowerMenu;
    bool m_showPowerDialogConfirmation;

    int m_ddcciBrightness;
    bool m_displayBatteryFooter;
    int m_panelStyle;
    int m_headsetcontrolFetchRate;
    bool m_enableNotifications;

    bool m_enableMediaOverlay;
    int m_mediaOverlayPosition;
    int m_mediaOverlaySize;

    int m_sliderWheelSensivity;

    bool m_enableInternalBattery;
};
