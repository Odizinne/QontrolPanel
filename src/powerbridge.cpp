#include "powerbridge.h"
#include "logmanager.h"
#include <windows.h>
#include <lm.h>
#include <powrprof.h>
#include <QGuiApplication>
#include <QWindow>

#define VARIABLE_ATTRIBUTE_NON_VOLATILE 0x00000001
#define VARIABLE_ATTRIBUTE_BOOTSERVICE_ACCESS 0x00000002
#define VARIABLE_ATTRIBUTE_RUNTIME_ACCESS 0x00000004

DEFINE_GUID(GUID_POWER_SAVING_STATUS, 0xE00958C0, 0xC213, 0x4ACE, 0xAC, 0x77, 0xFE, 0xCC, 0xED, 0x2E, 0xEE, 0xA5);

// Battery Saver overlay scheme GUID
static const GUID GUID_BATTERY_SAVER_OVERLAY = {0x961cc777, 0x2547, 0x4f9d, {0x81, 0x74, 0x7d, 0x86, 0x18, 0x1b, 0x8a, 0x7a}};

// Dynamic function loading for PowerSetActiveOverlayScheme
typedef DWORD (WINAPI *PowerSetActiveOverlaySchemeProc)(const GUID*);

PowerBridge* PowerBridge::m_instance = nullptr;

PowerBridge::PowerBridge(QObject* parent)
    : QObject(parent)
    , m_batteryLevel(0)
    , m_batteryStatus(Unknown)
    , m_batterySaverEnabled(false)
    , m_powerNotifyHandle(nullptr)
    , m_acNotifyHandle(nullptr)
    , m_batterySaverNotifyHandle(nullptr)
{
    updateBatteryStatus();
    updateBatterySaverStatus();
    startBatteryMonitoring();

    // Install native event filter
    qApp->installNativeEventFilter(this);
}

PowerBridge::~PowerBridge()
{
    qApp->removeNativeEventFilter(this);
    stopBatteryMonitoring();
    if (m_instance == this) {
        m_instance = nullptr;
    }
}

PowerBridge* PowerBridge::create(QQmlEngine* qmlEngine, QJSEngine* jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)

    if (!m_instance) {
        m_instance = new PowerBridge();
    }
    return m_instance;
}

PowerBridge* PowerBridge::instance()
{
    return m_instance;
}

bool PowerBridge::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(result)

    if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
        MSG* msg = static_cast<MSG*>(message);

        if (msg->message == WM_POWERBROADCAST) {
            if (msg->wParam == PBT_APMPOWERSTATUSCHANGE) {
                LOG_INFO("PowerManager", "Power status changed (PBT_APMPOWERSTATUSCHANGE)");
                updateBatteryStatus();
            } else if (msg->wParam == PBT_POWERSETTINGCHANGE) {
                POWERBROADCAST_SETTING* setting = reinterpret_cast<POWERBROADCAST_SETTING*>(msg->lParam);
                if (setting) {
                    if (IsEqualGUID(setting->PowerSetting, GUID_BATTERY_PERCENTAGE_REMAINING)) {
                        LOG_INFO("PowerManager", "Battery percentage changed");
                        updateBatteryStatus();
                    } else if (IsEqualGUID(setting->PowerSetting, GUID_ACDC_POWER_SOURCE)) {
                        LOG_INFO("PowerManager", "AC/DC power source changed");
                        updateBatteryStatus();
                    } else if (IsEqualGUID(setting->PowerSetting, GUID_POWER_SAVING_STATUS)) {
                        LOG_INFO("PowerManager", "Battery saver status changed");
                        updateBatterySaverStatus();
                    }
                }
            }
        }
    }

    return false;
}

void PowerBridge::updateBatteryStatus()
{
    SYSTEM_POWER_STATUS powerStatus;
    if (GetSystemPowerStatus(&powerStatus)) {
        // Update battery level
        int newLevel = 0;
        if (powerStatus.BatteryLifePercent != 255) {
            newLevel = powerStatus.BatteryLifePercent;
        }

        if (newLevel != m_batteryLevel) {
            m_batteryLevel = newLevel;
            emit batteryLevelChanged();
        }

        // Update battery status
        int newStatus = Unknown;
        if (powerStatus.BatteryFlag == 128) {
            // No system battery
            newStatus = NotPresent;
        } else if (powerStatus.BatteryFlag == 255) {
            // Unknown status
            newStatus = Unknown;
        } else if (powerStatus.ACLineStatus == 1) {
            // AC power online
            if (powerStatus.BatteryFlag & 8) {
                // Charging
                newStatus = Charging;
            } else if (powerStatus.BatteryFlag == 128 || powerStatus.BatteryFlag == 255) {
                // No battery present
                newStatus = ACPower;
            } else {
                // On AC but fully charged or not charging
                newStatus = Charging;
            }
        } else if (powerStatus.ACLineStatus == 0) {
            // AC power offline - discharging
            newStatus = Discharging;
        } else {
            // Unknown AC status
            newStatus = Unknown;
        }

        if (newStatus != m_batteryStatus) {
            m_batteryStatus = newStatus;
            emit batteryStatusChanged();
        }

        LOG_INFO("PowerManager",
                 QString("Battery: %1%, Status: %2, ACLine: %3, BatteryFlag: %4")
                     .arg(m_batteryLevel)
                     .arg(m_batteryStatus)
                     .arg(powerStatus.ACLineStatus)
                     .arg(powerStatus.BatteryFlag));
    } else {
        LOG_CRITICAL("PowerManager", "Failed to get system power status");
    }
}

void PowerBridge::updateBatterySaverStatus()
{
    SYSTEM_POWER_STATUS powerStatus;
    if (GetSystemPowerStatus(&powerStatus)) {
        // SystemStatusFlag indicates if Battery Saver is on
        // Non-zero means Battery Saver is ON
        bool newStatus = (powerStatus.SystemStatusFlag != 0);

        if (newStatus != m_batterySaverEnabled) {
            m_batterySaverEnabled = newStatus;
            emit batterySaverEnabledChanged();
            LOG_INFO("PowerManager",
                     QString("Battery saver status changed: %1").arg(newStatus ? "enabled" : "disabled"));
        }
    } else {
        LOG_CRITICAL("PowerManager", "Failed to get battery saver status");
    }
}

void PowerBridge::setBatterySaver(bool enable)
{
    // Load the function dynamically for compatibility
    HMODULE hPowrProf = LoadLibraryW(L"powrprof.dll");
    if (!hPowrProf) {
        LOG_CRITICAL("PowerManager", "Failed to load powrprof.dll");
        return;
    }

    PowerSetActiveOverlaySchemeProc pPowerSetActiveOverlayScheme =
        (PowerSetActiveOverlaySchemeProc)GetProcAddress(hPowrProf, "PowerSetActiveOverlayScheme");

    if (!pPowerSetActiveOverlayScheme) {
        LOG_CRITICAL("PowerManager", "PowerSetActiveOverlayScheme not available (requires Windows 10+)");
        FreeLibrary(hPowrProf);
        return;
    }

    DWORD result;
    if (enable) {
        // Enable battery saver overlay
        result = pPowerSetActiveOverlayScheme(&GUID_BATTERY_SAVER_OVERLAY);
    } else {
        // Disable battery saver overlay
        result = pPowerSetActiveOverlayScheme(NULL);
    }

    FreeLibrary(hPowrProf);

    if (result == ERROR_SUCCESS) {
        LOG_INFO("PowerManager",
                 QString("Battery saver %1 successfully").arg(enable ? "enabled" : "disabled"));
        updateBatterySaverStatus();
    } else {
        LOG_CRITICAL("PowerManager",
                     QString("Failed to set battery saver, error: %1").arg(result));
    }
}

void PowerBridge::startBatteryMonitoring()
{
    // Get the main window handle
    HWND hwnd = nullptr;

    // Try to get window from QGuiApplication
    QWindowList windows = QGuiApplication::topLevelWindows();
    if (!windows.isEmpty()) {
        hwnd = reinterpret_cast<HWND>(windows.first()->winId());
    }

    if (!hwnd) {
        // Fallback: use console window or create a message-only window
        hwnd = GetConsoleWindow();
        if (!hwnd) {
            LOG_CRITICAL("PowerManager", "No window handle available for power notifications");
            return;
        }
    }

    LOG_INFO("PowerManager", QString("Using window handle: %1").arg(reinterpret_cast<quintptr>(hwnd)));

    // Register for battery percentage changes
    m_powerNotifyHandle = RegisterPowerSettingNotification(
        hwnd,
        &GUID_BATTERY_PERCENTAGE_REMAINING,
        DEVICE_NOTIFY_WINDOW_HANDLE
        );

    if (m_powerNotifyHandle) {
        LOG_INFO("PowerManager", "Battery percentage monitoring started successfully");
    } else {
        LOG_CRITICAL("PowerManager",
                     QString("Failed to register for battery notifications, error: %1")
                         .arg(GetLastError()));
    }

    // Also register for AC/DC power source changes
    m_acNotifyHandle = RegisterPowerSettingNotification(
        hwnd,
        &GUID_ACDC_POWER_SOURCE,
        DEVICE_NOTIFY_WINDOW_HANDLE
        );

    if (m_acNotifyHandle) {
        LOG_INFO("PowerManager", "AC/DC power monitoring started successfully");
    } else {
        LOG_CRITICAL("PowerManager",
                     QString("Failed to register for AC/DC notifications, error: %1")
                         .arg(GetLastError()));
    }

    // Register for battery saver status changes
    m_batterySaverNotifyHandle = RegisterPowerSettingNotification(
        hwnd,
        &GUID_POWER_SAVING_STATUS,
        DEVICE_NOTIFY_WINDOW_HANDLE
        );

    if (m_batterySaverNotifyHandle) {
        LOG_INFO("PowerManager", "Battery saver monitoring started successfully");
    } else {
        LOG_CRITICAL("PowerManager",
                     QString("Failed to register for battery saver notifications, error: %1")
                         .arg(GetLastError()));
    }
}

void PowerBridge::stopBatteryMonitoring()
{
    if (m_powerNotifyHandle) {
        UnregisterPowerSettingNotification(m_powerNotifyHandle);
        m_powerNotifyHandle = nullptr;
    }

    if (m_acNotifyHandle) {
        UnregisterPowerSettingNotification(m_acNotifyHandle);
        m_acNotifyHandle = nullptr;
    }

    if (m_batterySaverNotifyHandle) {
        UnregisterPowerSettingNotification(m_batterySaverNotifyHandle);
        m_batterySaverNotifyHandle = nullptr;
    }

    LOG_INFO("PowerManager", "Battery monitoring stopped");
}

bool PowerBridge::enableShutdownPrivilege()
{
    HANDLE hToken;
    TOKEN_PRIVILEGES tkp;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return false;
    }

    LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);
    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES)NULL, 0);

    if (GetLastError() != ERROR_SUCCESS) {
        CloseHandle(hToken);
        return false;
    }

    CloseHandle(hToken);
    return true;
}

bool PowerBridge::hasMultipleUsers()
{
    LPUSER_INFO_1 buffer = nullptr;
    DWORD entriesRead = 0;
    DWORD totalEntries = 0;
    DWORD realUserCount = 0;
    NET_API_STATUS status = NetUserEnum(nullptr, 1, FILTER_NORMAL_ACCOUNT,
                                        (LPBYTE*)&buffer, MAX_PREFERRED_LENGTH,
                                        &entriesRead, &totalEntries, nullptr);
    if (status == NERR_Success) {
        LOG_INFO("PowerManager",
                 QString("Found %1 user accounts, analyzing...").arg(entriesRead));

        for (DWORD i = 0; i < entriesRead; i++) {
            QString username = QString::fromWCharArray(buffer[i].usri1_name);
            if (username.compare("Administrator", Qt::CaseInsensitive) != 0 &&
                username.compare("Guest", Qt::CaseInsensitive) != 0 &&
                username.compare("DefaultAccount", Qt::CaseInsensitive) != 0 &&
                username.compare("WDAGUtilityAccount", Qt::CaseInsensitive) != 0 &&
                !username.startsWith("_") &&
                !(buffer[i].usri1_flags & UF_ACCOUNTDISABLE)) {
                realUserCount++;
                LOG_INFO("PowerManager",
                         QString("Real user found: %1").arg(username));
            } else {
                LOG_INFO("PowerManager",
                         QString("Skipping system account: %1").arg(username));
            }
        }
        NetApiBufferFree(buffer);
        bool result = realUserCount > 1;
        LOG_INFO("PowerManager",
                 QString("Real users found: %1, hasMultipleUsers returning: %2")
                     .arg(realUserCount).arg(result ? "true" : "false"));
        return result;
    }

    LOG_CRITICAL("PowerManager",
                 QString("Failed to enumerate users, NetUserEnum error: %1").arg(status));
    return false;
}

bool PowerBridge::isHibernateSupported()
{
    SYSTEM_POWER_CAPABILITIES powerCaps;
    ZeroMemory(&powerCaps, sizeof(powerCaps));

    if (GetPwrCapabilities(&powerCaps)) {
        return powerCaps.SystemS4 && powerCaps.HiberFilePresent;
    }

    return false;
}

bool PowerBridge::isSleepSupported()
{
    SYSTEM_POWER_CAPABILITIES powerCaps;
    if (GetPwrCapabilities(&powerCaps)) {
        return powerCaps.SystemS1 || powerCaps.SystemS2 || powerCaps.SystemS3;
    }
    return false;
}

bool PowerBridge::isUEFISupported() {
    FIRMWARE_TYPE fwType = FirmwareTypeUnknown;
    GetFirmwareType(&fwType);
    return fwType == FirmwareTypeUefi;
}

void PowerBridge::restartToUEFI()
{
    if (!isUEFISupported()) {
        return;
    }

    ShellExecuteW(NULL, L"runas", L"shutdown", L"/r /fw /t 0", NULL, SW_HIDE);
}

bool PowerBridge::shutdown()
{
    if (!enableShutdownPrivilege()) {
        return false;
    }

    return ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCE, SHTDN_REASON_MAJOR_APPLICATION);
}

bool PowerBridge::restart()
{
    if (!enableShutdownPrivilege()) {
        return false;
    }

    return ExitWindowsEx(EWX_REBOOT | EWX_FORCE, SHTDN_REASON_MAJOR_APPLICATION);
}

bool PowerBridge::sleep()
{
    return SetSuspendState(FALSE, FALSE, FALSE);
}

bool PowerBridge::hibernate()
{
    return SetSuspendState(TRUE, FALSE, FALSE);
}

bool PowerBridge::lockAccount()
{
    return LockWorkStation();
}

bool PowerBridge::signOut()
{
    return ExitWindowsEx(EWX_LOGOFF, SHTDN_REASON_MAJOR_APPLICATION);
}

bool PowerBridge::switchAccount()
{
    return LockWorkStation();
}
