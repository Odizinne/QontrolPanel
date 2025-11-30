#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QtQml/qqmlregistration.h>
#include <QSettings>
#include <QScreen>
#include <QGuiApplication>
#include <QTranslator>
#include <QTimer>

class SoundPanelBridge : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString taskbarPosition READ taskbarPosition NOTIFY taskbarPositionChanged)

public:
    explicit SoundPanelBridge(QObject* parent = nullptr);
    ~SoundPanelBridge() override;

    static SoundPanelBridge* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);
    static SoundPanelBridge* instance();

    QString taskbarPosition() const;

    Q_INVOKABLE bool getShortcutState();
    Q_INVOKABLE void setStartupShortcut(bool enabled);
    Q_INVOKABLE QString getAppVersion() const;
    Q_INVOKABLE QString getQtVersion() const;
    Q_INVOKABLE QString getCommitHash() const;
    Q_INVOKABLE QString getBuildTimestamp() const;

    Q_INVOKABLE QString getCurrentLanguageCode() const;
    Q_INVOKABLE void changeApplicationLanguage(int languageIndex);
    Q_INVOKABLE QString getLanguageCodeFromIndex(int index) const;

    Q_INVOKABLE void toggleChatMixFromShortcut(bool enabled);
    Q_INVOKABLE void suspendGlobalShortcuts();
    Q_INVOKABLE void resumeGlobalShortcuts();
    bool areGlobalShortcutsSuspended() const;
    void requestChatMixNotification(QString message);
    Q_INVOKABLE QStringList getLanguageNativeNames() const;
    Q_INVOKABLE QStringList getLanguageCodes() const;
    Q_INVOKABLE void openLegacySoundSettings();
    Q_INVOKABLE void openModernSoundSettings();
    Q_INVOKABLE int getAvailableDesktopWidth() const;
    Q_INVOKABLE int getAvailableDesktopHeight() const;
    Q_INVOKABLE void playFeedbackSound();
    Q_INVOKABLE void setStyle(int style);

signals:
    void taskbarPositionChanged();
    void languageChanged();
    void chatMixEnabledChanged(bool enabled);
    void chatMixNotificationRequested(QString message);

private:
    static SoundPanelBridge* m_instance;
    QSettings settings;
    QString detectTaskbarPosition() const;
    QTranslator *translator;
    bool m_globalShortcutsSuspended = false;
};
