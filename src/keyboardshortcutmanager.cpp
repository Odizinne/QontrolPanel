#include "keyboardshortcutmanager.h"
#include "usersettings.h"
#include <QCoreApplication>
#include <QWindow>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

KeyboardShortcutManager* KeyboardShortcutManager::m_instance = nullptr;

KeyboardShortcutManager* KeyboardShortcutManager::instance()
{
    if (!m_instance) {
        m_instance = new KeyboardShortcutManager();
    }
    return m_instance;
}

KeyboardShortcutManager* KeyboardShortcutManager::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    return instance();
}

KeyboardShortcutManager::KeyboardShortcutManager(QObject *parent)
    : QObject(parent)
{
    // Get a window handle - we'll use a message-only window
    m_hwnd = CreateWindowEx(0, L"STATIC", L"QontrolPanelHotkeys",
                            0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);

    // Install the native event filter
    QCoreApplication::instance()->installNativeEventFilter(this);

    // Load per-app volume hotkeys from file
    loadAppVolumeHotkeys();

    if (UserSettings::instance()->globalShortcutsEnabled()) {
        registerHotkeys();
    }
}

KeyboardShortcutManager::~KeyboardShortcutManager()
{
    unregisterHotkeys();

    // Remove the native event filter
    QCoreApplication::instance()->removeNativeEventFilter(this);

    // Destroy the message-only window
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }

    if (m_instance == this) {
        m_instance = nullptr;
    }
}

void KeyboardShortcutManager::manageGlobalShortcuts(bool enabled)
{
    if (enabled) {
        registerHotkeys();
    } else {
        unregisterHotkeys();
    }
}

bool KeyboardShortcutManager::globalShortcutsSuspended() const
{
    return m_globalShortcutsSuspended;
}

void KeyboardShortcutManager::setGlobalShortcutsSuspended(bool suspended)
{
    if (m_globalShortcutsSuspended == suspended)
        return;

    m_globalShortcutsSuspended = suspended;
    emit globalShortcutsSuspendedChanged();
}

void KeyboardShortcutManager::toggleChatMixFromShortcut(bool enabled)
{
    emit chatMixEnabledChanged(enabled);
}

void KeyboardShortcutManager::registerHotkeys()
{
    if (!m_hwnd) return;

    // Unregister any existing hotkeys first
    unregisterHotkeys();

    // Register panel toggle hotkey
    UINT panelMods = convertQtModifiers(UserSettings::instance()->panelShortcutModifiers());
    UINT panelKey = qtKeyToVirtualKey(UserSettings::instance()->panelShortcutKey());
    if (panelKey != 0 && RegisterHotKey(m_hwnd, HOTKEY_PANEL_TOGGLE, panelMods, panelKey)) {
        m_registeredHotkeys[HOTKEY_PANEL_TOGGLE] = true;
    }

    // Register ChatMix toggle hotkey
    UINT chatMixMods = convertQtModifiers(UserSettings::instance()->chatMixShortcutModifiers());
    UINT chatMixKey = qtKeyToVirtualKey(UserSettings::instance()->chatMixShortcutKey());
    if (chatMixKey != 0 && RegisterHotKey(m_hwnd, HOTKEY_CHATMIX_TOGGLE, chatMixMods, chatMixKey)) {
        m_registeredHotkeys[HOTKEY_CHATMIX_TOGGLE] = true;
    }

    // Register mic mute hotkey
    UINT micMuteMods = convertQtModifiers(UserSettings::instance()->micMuteShortcutModifiers());
    UINT micMuteKey = qtKeyToVirtualKey(UserSettings::instance()->micMuteShortcutKey());
    if (micMuteKey != 0 && RegisterHotKey(m_hwnd, HOTKEY_MIC_MUTE, micMuteMods, micMuteKey)) {
        m_registeredHotkeys[HOTKEY_MIC_MUTE] = true;
    }

    // Register per-app volume hotkeys
    registerAppVolumeHotkeys();
}

void KeyboardShortcutManager::unregisterHotkeys()
{
    if (!m_hwnd) return;

    for (auto it = m_registeredHotkeys.begin(); it != m_registeredHotkeys.end(); ++it) {
        UnregisterHotKey(m_hwnd, it.key());
    }
    m_registeredHotkeys.clear();
}

void KeyboardShortcutManager::updateHotkey(HotkeyId id, int qtKey, int qtMods)
{
    if (!m_hwnd) return;

    // Unregister the old hotkey
    if (m_registeredHotkeys.contains(id)) {
        UnregisterHotKey(m_hwnd, id);
        m_registeredHotkeys.remove(id);
    }

    // Register the new hotkey
    UINT mods = convertQtModifiers(qtMods);
    UINT key = qtKeyToVirtualKey(qtKey);
    if (key != 0 && RegisterHotKey(m_hwnd, id, mods, key)) {
        m_registeredHotkeys[id] = true;
    }
}

UINT KeyboardShortcutManager::convertQtModifiers(int qtMods)
{
    UINT winMods = 0;
    if (qtMods & Qt::ControlModifier) winMods |= MOD_CONTROL;
    if (qtMods & Qt::ShiftModifier) winMods |= MOD_SHIFT;
    if (qtMods & Qt::AltModifier) winMods |= MOD_ALT;
    return winMods;
}

bool KeyboardShortcutManager::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(result)

    if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
        MSG* msg = static_cast<MSG*>(message);

        if (msg->message == WM_HOTKEY) {
            // Check if shortcuts are suspended
            if (m_globalShortcutsSuspended) {
                return false;
            }

            int hotkeyId = static_cast<int>(msg->wParam);

            // Handle the hotkey based on its ID
            switch (hotkeyId) {
                case HOTKEY_PANEL_TOGGLE:
                    emit panelToggleRequested();
                    return true;
                case HOTKEY_CHATMIX_TOGGLE:
                    emit chatMixToggleRequested();
                    return true;
                case HOTKEY_MIC_MUTE:
                    emit micMuteToggleRequested();
                    return true;
            }

            // Check per-app volume hotkeys
            for (const auto &appHotkey : m_appVolumeHotkeys) {
                if (hotkeyId == appHotkey.volumeUpHotkeyId) {
                    emit appVolumeHotkeyPressed(appHotkey.executableName, true, appHotkey.volumeStepSize);
                    return true;
                }
                if (hotkeyId == appHotkey.volumeDownHotkeyId) {
                    emit appVolumeHotkeyPressed(appHotkey.executableName, false, appHotkey.volumeStepSize);
                    return true;
                }
            }
        }
    }

    return false;
}

int KeyboardShortcutManager::qtKeyToVirtualKey(int qtKey)
{
    switch (qtKey) {
    case Qt::Key_A: return 0x41;
    case Qt::Key_B: return 0x42;
    case Qt::Key_C: return 0x43;
    case Qt::Key_D: return 0x44;
    case Qt::Key_E: return 0x45;
    case Qt::Key_F: return 0x46;
    case Qt::Key_G: return 0x47;
    case Qt::Key_H: return 0x48;
    case Qt::Key_I: return 0x49;
    case Qt::Key_J: return 0x4A;
    case Qt::Key_K: return 0x4B;
    case Qt::Key_L: return 0x4C;
    case Qt::Key_M: return 0x4D;
    case Qt::Key_N: return 0x4E;
    case Qt::Key_O: return 0x4F;
    case Qt::Key_P: return 0x50;
    case Qt::Key_Q: return 0x51;
    case Qt::Key_R: return 0x52;
    case Qt::Key_S: return 0x53;
    case Qt::Key_T: return 0x54;
    case Qt::Key_U: return 0x55;
    case Qt::Key_V: return 0x56;
    case Qt::Key_W: return 0x57;
    case Qt::Key_X: return 0x58;
    case Qt::Key_Y: return 0x59;
    case Qt::Key_Z: return 0x5A;
    case Qt::Key_F1: return VK_F1;
    case Qt::Key_F2: return VK_F2;
    case Qt::Key_F3: return VK_F3;
    case Qt::Key_F4: return VK_F4;
    case Qt::Key_F5: return VK_F5;
    case Qt::Key_F6: return VK_F6;
    case Qt::Key_F7: return VK_F7;
    case Qt::Key_F8: return VK_F8;
    case Qt::Key_F9: return VK_F9;
    case Qt::Key_F10: return VK_F10;
    case Qt::Key_F11: return VK_F11;
    case Qt::Key_F12: return VK_F12;
    case Qt::Key_F13: return VK_F13;
    case Qt::Key_F14: return VK_F14;
    case Qt::Key_F15: return VK_F15;
    case Qt::Key_F16: return VK_F16;
    case Qt::Key_F17: return VK_F17;
    case Qt::Key_F18: return VK_F18;
    case Qt::Key_F19: return VK_F19;
    case Qt::Key_F20: return VK_F20;
    case Qt::Key_F21: return VK_F21;
    case Qt::Key_F22: return VK_F22;
    case Qt::Key_F23: return VK_F23;
    case Qt::Key_F24: return VK_F24;
    case Qt::Key_0: return 0x30;
    case Qt::Key_1: return 0x31;
    case Qt::Key_2: return 0x32;
    case Qt::Key_3: return 0x33;
    case Qt::Key_4: return 0x34;
    case Qt::Key_5: return 0x35;
    case Qt::Key_6: return 0x36;
    case Qt::Key_7: return 0x37;
    case Qt::Key_8: return 0x38;
    case Qt::Key_9: return 0x39;
    case Qt::Key_Up: return VK_UP;
    case Qt::Key_Down: return VK_DOWN;
    case Qt::Key_Left: return VK_LEFT;
    case Qt::Key_Right: return VK_RIGHT;
    case Qt::Key_Space: return VK_SPACE;
    case Qt::Key_Return: return VK_RETURN;
    case Qt::Key_Enter: return VK_RETURN;
    case Qt::Key_Tab: return VK_TAB;
    case Qt::Key_Escape: return VK_ESCAPE;
    case Qt::Key_Backspace: return VK_BACK;
    case Qt::Key_Delete: return VK_DELETE;
    case Qt::Key_Insert: return VK_INSERT;
    case Qt::Key_Home: return VK_HOME;
    case Qt::Key_End: return VK_END;
    case Qt::Key_PageUp: return VK_PRIOR;
    case Qt::Key_PageDown: return VK_NEXT;
    default: return 0;
    }
}

// Per-app volume hotkey methods

QString KeyboardShortcutManager::getAppVolumeHotkeysFilePath() const
{
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appDataPath);
    return appDataPath + "/appvolumehotkeys.json";
}

void KeyboardShortcutManager::loadAppVolumeHotkeys()
{
    QFile file(getAppVolumeHotkeysFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isArray())
        return;

    QJsonArray arr = doc.array();
    for (const QJsonValue &val : arr) {
        QJsonObject obj = val.toObject();
        AppVolumeHotkey hotkey;
        hotkey.executableName = obj["executableName"].toString();
        hotkey.volumeUpKey = obj["volumeUpKey"].toInt();
        hotkey.volumeUpModifiers = obj["volumeUpModifiers"].toInt();
        hotkey.volumeDownKey = obj["volumeDownKey"].toInt();
        hotkey.volumeDownModifiers = obj["volumeDownModifiers"].toInt();
        hotkey.volumeStepSize = obj["volumeStepSize"].toInt(0);
        m_appVolumeHotkeys.append(hotkey);
    }
}

void KeyboardShortcutManager::saveAppVolumeHotkeys()
{
    QJsonArray arr;
    for (const auto &hotkey : m_appVolumeHotkeys) {
        QJsonObject obj;
        obj["executableName"] = hotkey.executableName;
        obj["volumeUpKey"] = hotkey.volumeUpKey;
        obj["volumeUpModifiers"] = hotkey.volumeUpModifiers;
        obj["volumeDownKey"] = hotkey.volumeDownKey;
        obj["volumeDownModifiers"] = hotkey.volumeDownModifiers;
        obj["volumeStepSize"] = hotkey.volumeStepSize;
        arr.append(obj);
    }

    QFile file(getAppVolumeHotkeysFilePath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(arr).toJson());
        file.close();
    }
}

void KeyboardShortcutManager::registerAppVolumeHotkeys()
{
    if (!m_hwnd) return;

    m_nextAppHotkeyId = APP_HOTKEY_BASE_ID;

    for (auto &hotkey : m_appVolumeHotkeys) {
        // Register volume up
        UINT upMods = convertQtModifiers(hotkey.volumeUpModifiers);
        UINT upKey = qtKeyToVirtualKey(hotkey.volumeUpKey);
        hotkey.volumeUpHotkeyId = m_nextAppHotkeyId++;
        if (upKey != 0 && RegisterHotKey(m_hwnd, hotkey.volumeUpHotkeyId, upMods, upKey)) {
            m_registeredHotkeys[hotkey.volumeUpHotkeyId] = true;
        }

        // Register volume down
        UINT downMods = convertQtModifiers(hotkey.volumeDownModifiers);
        UINT downKey = qtKeyToVirtualKey(hotkey.volumeDownKey);
        hotkey.volumeDownHotkeyId = m_nextAppHotkeyId++;
        if (downKey != 0 && RegisterHotKey(m_hwnd, hotkey.volumeDownHotkeyId, downMods, downKey)) {
            m_registeredHotkeys[hotkey.volumeDownHotkeyId] = true;
        }
    }
}

void KeyboardShortcutManager::unregisterAppVolumeHotkeys()
{
    if (!m_hwnd) return;

    for (const auto &hotkey : m_appVolumeHotkeys) {
        if (m_registeredHotkeys.contains(hotkey.volumeUpHotkeyId)) {
            UnregisterHotKey(m_hwnd, hotkey.volumeUpHotkeyId);
            m_registeredHotkeys.remove(hotkey.volumeUpHotkeyId);
        }
        if (m_registeredHotkeys.contains(hotkey.volumeDownHotkeyId)) {
            UnregisterHotKey(m_hwnd, hotkey.volumeDownHotkeyId);
            m_registeredHotkeys.remove(hotkey.volumeDownHotkeyId);
        }
    }
}

bool KeyboardShortcutManager::addAppVolumeHotkey(const QString &executableName, int volUpKey, int volUpMods, int volDownKey, int volDownMods, int volumeStepSize)
{
    // Remove existing hotkey for this app if any
    removeAppVolumeHotkey(executableName);

    AppVolumeHotkey hotkey;
    hotkey.executableName = executableName;
    hotkey.volumeUpKey = volUpKey;
    hotkey.volumeUpModifiers = volUpMods;
    hotkey.volumeDownKey = volDownKey;
    hotkey.volumeDownModifiers = volDownMods;
    hotkey.volumeStepSize = volumeStepSize;

    m_appVolumeHotkeys.append(hotkey);
    saveAppVolumeHotkeys();

    // Re-register all hotkeys if global shortcuts are enabled
    if (UserSettings::instance()->globalShortcutsEnabled()) {
        manageGlobalShortcuts(true);
    }

    emit appVolumeHotkeysChanged();
    return true;
}

void KeyboardShortcutManager::removeAppVolumeHotkey(const QString &executableName)
{
    for (int i = 0; i < m_appVolumeHotkeys.size(); ++i) {
        if (m_appVolumeHotkeys[i].executableName == executableName) {
            // Unregister these specific hotkeys
            if (m_hwnd) {
                if (m_registeredHotkeys.contains(m_appVolumeHotkeys[i].volumeUpHotkeyId)) {
                    UnregisterHotKey(m_hwnd, m_appVolumeHotkeys[i].volumeUpHotkeyId);
                    m_registeredHotkeys.remove(m_appVolumeHotkeys[i].volumeUpHotkeyId);
                }
                if (m_registeredHotkeys.contains(m_appVolumeHotkeys[i].volumeDownHotkeyId)) {
                    UnregisterHotKey(m_hwnd, m_appVolumeHotkeys[i].volumeDownHotkeyId);
                    m_registeredHotkeys.remove(m_appVolumeHotkeys[i].volumeDownHotkeyId);
                }
            }
            m_appVolumeHotkeys.removeAt(i);
            saveAppVolumeHotkeys();
            emit appVolumeHotkeysChanged();
            return;
        }
    }
}

QJsonArray KeyboardShortcutManager::appVolumeHotkeysJson() const
{
    QJsonArray arr;
    for (const auto &hotkey : m_appVolumeHotkeys) {
        QJsonObject obj;
        obj["executableName"] = hotkey.executableName;
        obj["volumeUpKey"] = hotkey.volumeUpKey;
        obj["volumeUpModifiers"] = hotkey.volumeUpModifiers;
        obj["volumeDownKey"] = hotkey.volumeDownKey;
        obj["volumeDownModifiers"] = hotkey.volumeDownModifiers;
        obj["volumeStepSize"] = hotkey.volumeStepSize;
        arr.append(obj);
    }
    return arr;
}
