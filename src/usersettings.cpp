#include "usersettings.h"
#include <QSettings>

UserSettings* UserSettings::m_instance = nullptr;

UserSettings::UserSettings(QObject *parent)
    : QObject(parent)
{
    initProperties();
}

UserSettings* UserSettings::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    return instance();
}

UserSettings* UserSettings::instance()
{
    if (!m_instance) {
        m_instance = new UserSettings();
    }
    return m_instance;
}

void UserSettings::saveValue(const QString &key, const QVariant &value)
{
    QSettings settings("Odizinne", "QontrolPanel");
    settings.setValue(key, value);
}

void UserSettings::initProperties()
{
    QSettings settings("Odizinne", "QontrolPanel");

    m_enableDeviceManager = settings.value("enableDeviceManager", true).toBool();
    m_enableApplicationMixer = settings.value("enableApplicationMixer", true).toBool();
    m_enableMediaSessionManager = settings.value("enableMediaSessionManager", true).toBool();
    m_panelPosition = settings.value("panelPosition", 1).toInt();
    m_taskbarOffset = settings.value("taskbarOffset", 0).toInt();
    m_xAxisMargin = settings.value("xAxisMargin", 12).toInt();
    m_yAxisMargin = settings.value("yAxisMargin", 12).toInt();
    m_languageIndex = settings.value("languageIndex", 0).toInt();

    m_commApps = settings.value("commApps", QVariantList()).toList();
    m_chatMixValue = settings.value("chatMixValue", 50).toInt();
    m_chatMixEnabled = settings.value("chatMixEnabled", false).toBool();
    m_activateChatmix = settings.value("activateChatmix", false).toBool();
    m_showAudioLevel = settings.value("showAudioLevel", true).toBool();
    m_chatmixRestoreVolume = settings.value("chatmixRestoreVolume", 80).toInt();

    m_globalShortcutsEnabled = settings.value("globalShortcutsEnabled", true).toBool();
    m_panelShortcutKey = settings.value("panelShortcutKey", 83).toInt();
    m_panelShortcutModifiers = settings.value("panelShortcutModifiers", 117440512).toInt();
    m_chatMixShortcutKey = settings.value("chatMixShortcutKey", 77).toInt();
    m_chatMixShortcutModifiers = settings.value("chatMixShortcutModifiers", 117440512).toInt();
    m_chatMixShortcutNotification = settings.value("chatMixShortcutNotification", true).toBool();
    m_micMuteShortcutKey = settings.value("micMuteShortcutKey", 75).toInt();
    m_micMuteShortcutModifiers = settings.value("micMuteShortcutModifiers", 117440512).toInt();
    m_autoUpdateTranslations = settings.value("autoUpdateTranslations", false).toBool();
    m_firstRun = settings.value("firstRun", true).toBool();

    m_trayIconTheme = settings.value("trayIconTheme", 0).toInt();
    m_iconStyle = settings.value("iconStyle", 0).toInt();

    m_autoFetchForAppUpdates = settings.value("autoFetchForAppUpdates", false).toBool();
    m_headsetcontrolMonitoring = settings.value("headsetcontrolMonitoring", true).toBool();
    m_headsetcontrolLights = settings.value("headsetcontrolLights", true).toBool();
    m_headsetcontrolSidetone = settings.value("headsetcontrolSidetone", 0).toInt();
    m_allowBrightnessControl = settings.value("allowBrightnessControl", true).toBool();
    m_avoidApplicationsOverflow = settings.value("avoidApplicationsOverflow", false).toBool();
    m_ddcciQueueDelay = settings.value("ddcciQueueDelay", 500).toInt();

    m_enablePowerMenu = settings.value("enablePowerMenu", true).toBool();
    m_showPowerDialogConfirmation = settings.value("showPowerDialogConfirmation", true).toBool();

    m_ddcciBrightness = settings.value("ddcciBrightness", 100).toInt();
    m_displayBatteryFooter = settings.value("displayBatteryFooter", true).toBool();
    m_panelStyle = settings.value("panelStyle", 0).toInt();
    m_headsetcontrolFetchRate = settings.value("headsetcontrolFetchRate", 20).toInt();
    m_enableNotifications = settings.value("enableNotifications", false).toBool();

    m_enableMediaOverlay = settings.value("enableMediaOverlay", false).toBool();
    m_mediaOverlayPosition = settings.value("mediaOverlayPosition", 1).toInt(); // Default: top-center
    m_mediaOverlaySize = settings.value("mediaOverlaySize", 1).toInt(); // Default: normal

    m_sliderWheelSensivity = settings.value("sliderWheelSensivity", 2).toInt();
}

// Setters
void UserSettings::setEnableDeviceManager(bool value)
{
    if (m_enableDeviceManager != value) {
        m_enableDeviceManager = value;
        saveValue("enableDeviceManager", value);
        emit enableDeviceManagerChanged();
    }
}

void UserSettings::setEnableApplicationMixer(bool value)
{
    if (m_enableApplicationMixer != value) {
        m_enableApplicationMixer = value;
        saveValue("enableApplicationMixer", value);
        emit enableApplicationMixerChanged();
    }
}

void UserSettings::setEnableMediaSessionManager(bool value)
{
    if (m_enableMediaSessionManager != value) {
        m_enableMediaSessionManager = value;
        saveValue("enableMediaSessionManager", value);
        emit enableMediaSessionManagerChanged();
    }
}

void UserSettings::setPanelPosition(int value)
{
    if (m_panelPosition != value) {
        m_panelPosition = value;
        saveValue("panelPosition", value);
        emit panelPositionChanged();
    }
}

void UserSettings::setTaskbarOffset(int value)
{
    if (m_taskbarOffset != value) {
        m_taskbarOffset = value;
        saveValue("taskbarOffset", value);
        emit taskbarOffsetChanged();
    }
}

void UserSettings::setXAxisMargin(int value)
{
    if (m_xAxisMargin != value) {
        m_xAxisMargin = value;
        saveValue("xAxisMargin", value);
        emit xAxisMarginChanged();
    }
}

void UserSettings::setYAxisMargin(int value)
{
    if (m_yAxisMargin != value) {
        m_yAxisMargin = value;
        saveValue("yAxisMargin", value);
        emit yAxisMarginChanged();
    }
}

void UserSettings::setLanguageIndex(int value)
{
    if (m_languageIndex != value) {
        m_languageIndex = value;
        saveValue("languageIndex", value);
        emit languageIndexChanged();
    }
}

void UserSettings::setCommApps(const QVariantList &value)
{
    if (m_commApps != value) {
        m_commApps = value;
        saveValue("commApps", value);
        emit commAppsChanged();
    }
}

void UserSettings::setChatMixValue(int value)
{
    if (m_chatMixValue != value) {
        m_chatMixValue = value;
        saveValue("chatMixValue", value);
        emit chatMixValueChanged();
    }
}

void UserSettings::setChatMixEnabled(bool value)
{
    if (m_chatMixEnabled != value) {
        m_chatMixEnabled = value;
        saveValue("chatMixEnabled", value);
        emit chatMixEnabledChanged();
    }
}

void UserSettings::setActivateChatmix(bool value)
{
    if (m_activateChatmix != value) {
        m_activateChatmix = value;
        saveValue("activateChatmix", value);
        emit activateChatmixChanged();
    }
}

void UserSettings::setShowAudioLevel(bool value)
{
    if (m_showAudioLevel != value) {
        m_showAudioLevel = value;
        saveValue("showAudioLevel", value);
        emit showAudioLevelChanged();
    }
}

void UserSettings::setChatmixRestoreVolume(int value)
{
    if (m_chatmixRestoreVolume != value) {
        m_chatmixRestoreVolume = value;
        saveValue("chatmixRestoreVolume", value);
        emit chatmixRestoreVolumeChanged();
    }
}

void UserSettings::setGlobalShortcutsEnabled(bool value)
{
    if (m_globalShortcutsEnabled != value) {
        m_globalShortcutsEnabled = value;
        saveValue("globalShortcutsEnabled", value);
        emit globalShortcutsEnabledChanged();
    }
}

void UserSettings::setPanelShortcutKey(int value)
{
    if (m_panelShortcutKey != value) {
        m_panelShortcutKey = value;
        saveValue("panelShortcutKey", value);
        emit panelShortcutKeyChanged();
    }
}

void UserSettings::setPanelShortcutModifiers(int value)
{
    if (m_panelShortcutModifiers != value) {
        m_panelShortcutModifiers = value;
        saveValue("panelShortcutModifiers", value);
        emit panelShortcutModifiersChanged();
    }
}

void UserSettings::setChatMixShortcutKey(int value)
{
    if (m_chatMixShortcutKey != value) {
        m_chatMixShortcutKey = value;
        saveValue("chatMixShortcutKey", value);
        emit chatMixShortcutKeyChanged();
    }
}

void UserSettings::setChatMixShortcutModifiers(int value)
{
    if (m_chatMixShortcutModifiers != value) {
        m_chatMixShortcutModifiers = value;
        saveValue("chatMixShortcutModifiers", value);
        emit chatMixShortcutModifiersChanged();
    }
}

void UserSettings::setChatMixShortcutNotification(bool value)
{
    if (m_chatMixShortcutNotification != value) {
        m_chatMixShortcutNotification = value;
        saveValue("chatMixShortcutNotification", value);
        emit chatMixShortcutNotificationChanged();
    }
}

void UserSettings::setMicMuteShortcutKey(int value)
{
    if (m_micMuteShortcutKey != value) {
        m_micMuteShortcutKey = value;
        saveValue("micMuteShortcutKey", value);
        emit micMuteShortcutKeyChanged();
    }
}

void UserSettings::setMicMuteShortcutModifiers(int value)
{
    if (m_micMuteShortcutModifiers != value) {
        m_micMuteShortcutModifiers = value;
        saveValue("micMuteShortcutModifiers", value);
        emit micMuteShortcutModifiersChanged();
    }
}

void UserSettings::setAutoUpdateTranslations(bool value)
{
    if (m_autoUpdateTranslations != value) {
        m_autoUpdateTranslations = value;
        saveValue("autoUpdateTranslations", value);
        emit autoUpdateTranslationsChanged();
    }
}

void UserSettings::setFirstRun(bool value)
{
    if (m_firstRun != value) {
        m_firstRun = value;
        saveValue("firstRun", value);
        emit firstRunChanged();
    }
}

void UserSettings::setTrayIconTheme(int value)
{
    if (m_trayIconTheme != value) {
        m_trayIconTheme = value;
        saveValue("trayIconTheme", value);
        emit trayIconThemeChanged();
    }
}

void UserSettings::setIconStyle(int value)
{
    if (m_iconStyle != value) {
        m_iconStyle = value;
        saveValue("iconStyle", value);
        emit iconStyleChanged();
    }
}

void UserSettings::setAutoFetchForAppUpdates(bool value)
{
    if (m_autoFetchForAppUpdates != value) {
        m_autoFetchForAppUpdates = value;
        saveValue("autoFetchForAppUpdates", value);
        emit autoFetchForAppUpdatesChanged();
    }
}

void UserSettings::setHeadsetcontrolMonitoring(bool value)
{
    if (m_headsetcontrolMonitoring != value) {
        m_headsetcontrolMonitoring = value;
        saveValue("headsetcontrolMonitoring", value);
        emit headsetcontrolMonitoringChanged();
    }
}

void UserSettings::setHeadsetcontrolLights(bool value)
{
    if (m_headsetcontrolLights != value) {
        m_headsetcontrolLights = value;
        saveValue("headsetcontrolLights", value);
        emit headsetcontrolLightsChanged();
    }
}

void UserSettings::setHeadsetcontrolSidetone(int value)
{
    if (m_headsetcontrolSidetone != value) {
        m_headsetcontrolSidetone = value;
        saveValue("headsetcontrolSidetone", value);
        emit headsetcontrolSidetoneChanged();
    }
}

void UserSettings::setAllowBrightnessControl(bool value)
{
    if (m_allowBrightnessControl != value) {
        m_allowBrightnessControl = value;
        saveValue("allowBrightnessControl", value);
        emit allowBrightnessControlChanged();
    }
}

void UserSettings::setAvoidApplicationsOverflow(bool value)
{
    if (m_avoidApplicationsOverflow != value) {
        m_avoidApplicationsOverflow = value;
        saveValue("avoidApplicationsOverflow", value);
        emit avoidApplicationsOverflowChanged();
    }
}

void UserSettings::setDdcciQueueDelay(int value)
{
    if (m_ddcciQueueDelay != value) {
        m_ddcciQueueDelay = value;
        saveValue("ddcciQueueDelay", value);
        emit ddcciQueueDelayChanged();
    }
}

void UserSettings::setEnablePowerMenu(bool value)
{
    if (m_enablePowerMenu != value) {
        m_enablePowerMenu = value;
        saveValue("enablePowerMenu", value);
        emit enablePowerMenuChanged();
    }
}

void UserSettings::setShowPowerDialogConfirmation(bool value)
{
    if (m_showPowerDialogConfirmation != value) {
        m_showPowerDialogConfirmation = value;
        saveValue("showPowerDialogConfirmation", value);
        emit showPowerDialogConfirmationChanged();
    }
}

void UserSettings::setDdcciBrightness(int value)
{
    if (m_ddcciBrightness != value) {
        m_ddcciBrightness = value;
        saveValue("ddcciBrightness", value);
        emit ddcciBrightnessChanged();
    }
}

void UserSettings::setDisplayBatteryFooter(bool value)
{
    if (m_displayBatteryFooter != value) {
        m_displayBatteryFooter = value;
        saveValue("displayBatteryFooter", value);
        emit displayBatteryFooterChanged();
    }
}

void UserSettings::setPanelStyle(int value)
{
    if (m_panelStyle != value) {
        m_panelStyle = value;
        saveValue("panelStyle", value);
        emit panelStyleChanged();
    }
}

void UserSettings::setHeadsetcontrolFetchRate(int value)
{
    if (m_headsetcontrolFetchRate != value) {
        m_headsetcontrolFetchRate = value;
        saveValue("headsetcontrolFetchRate", value);
        emit headsetcontrolFetchRateChanged();
    }
}

void UserSettings::setEnableNotifications(bool value)
{
    if (m_enableNotifications != value) {
        m_enableNotifications = value;
        saveValue("enableNotifications", value);
        emit enableNotificationsChanged();
    }
}

void UserSettings::setEnableMediaOverlay(bool value)
{
    if (m_enableMediaOverlay != value) {
        m_enableMediaOverlay = value;
        saveValue("enableMediaOverlay", value);
        emit enableMediaOverlayChanged();
    }
}

void UserSettings::setMediaOverlayPosition(int value)
{
    if (m_mediaOverlayPosition != value) {
        m_mediaOverlayPosition = value;
        saveValue("mediaOverlayPosition", value);
        emit mediaOverlayPositionChanged();
    }
}

void UserSettings::setMediaOverlaySize(int value)
{
    if (m_mediaOverlaySize != value) {
        m_mediaOverlaySize = value;
        saveValue("mediaOverlaySize", value);
        emit mediaOverlaySizeChanged();
    }
}

void UserSettings::setSliderWheelSensivity(int value)
{
    if (m_sliderWheelSensivity != value) {
        m_sliderWheelSensivity = value;
        saveValue("sliderWheelSensivity", value);
        emit sliderWheelSensivityChanged();
    }
}
