#ifndef QUICKSOUNDSWITCHER_H
#define QUICKSOUNDSWITCHER_H

#include "MediaFlyout.h"
#include "MediaSessionWorker.h"
#include "Panel.h"
#include "SettingsPage.h"
#include "SoundOverlay.h"
#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QSettings>

class QuickSoundSwitcher : public QMainWindow
{
    Q_OBJECT

public:
    QuickSoundSwitcher(QWidget *parent = nullptr);
    ~QuickSoundSwitcher();
    static QuickSoundSwitcher* instance;
    void adjustOutputVolume(bool up);
    void toggleMuteWithKey();

protected:
    bool event(QEvent *event) override;

private slots:
    void onPanelClosed();
    void onSoundOverlayClosed();
    void onRunAtStartupStateChanged();
    void onSessionReady(const MediaSession& session);
    void onVolumeChanged();
    void onOutputMuteChanged();

private:
    QSystemTrayIcon *trayIcon;
    QSettings settings;
    MediaSessionWorker* worker;
    Panel* panel;
    MediaFlyout* mediaFlyout;
    SoundOverlay* soundOverlay;
    void createTrayIcon();
    void showPanel();
    void hidePanel();
    void trayIconActivated(QSystemTrayIcon::ActivationReason reason);
    static LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    static HHOOK mouseHook;
    static HHOOK keyboardHook;
    void installGlobalMouseHook();
    void uninstallGlobalMouseHook();
    void installKeyboardHook();
    void uninstallKeyboardHook();

    static const int HOTKEY_ID = 1;
    void loadSettings();
    void onSettingsChanged();
    void onSettingsClosed();
    void showSettings();
    void updateApplicationColorScheme();
    bool mergeSimilarApps;
    int volumeIncrement;
    SettingsPage *settingsPage;
    QThread* workerThread;
    void getMediaSession();

signals:
    void muteStateChanged();
    void outputMuteStateChanged(bool state);
    void volumeChangedWithTray();
};

#endif // QUICKSOUNDSWITCHER_H
