#include "powerbridge.h"
#include "logmanager.h"
#include <windows.h>
#include <lm.h>
#include <powrprof.h>

#define VARIABLE_ATTRIBUTE_NON_VOLATILE 0x00000001
#define VARIABLE_ATTRIBUTE_BOOTSERVICE_ACCESS 0x00000002
#define VARIABLE_ATTRIBUTE_RUNTIME_ACCESS 0x00000004

PowerBridge* PowerBridge::m_instance = nullptr;

PowerBridge::PowerBridge(QObject* parent)
    : QObject(parent)
    , m_batteryLevel(0)
    , m_batteryStatus(Unknown)
    , m_powerNotifyHandle(NULL)
{
    updateBatteryStatus();
    startBatteryMonitoring();
}

PowerBridge::~PowerBridge()
{
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
        LOG_ERROR("PowerManager", "Failed to get system power status");
    }
}

static ULONG CALLBACK PowerSettingCallback(PVOID Context, ULONG Type, PVOID Setting)
{
    Q_UNUSED(Type)
    Q_UNUSED(Setting)
    
    PowerBridge* bridge = static_cast<PowerBridge*>(Context);
    if (bridge) {
        bridge->updateBatteryStatus();
    }
    return 0;
}

void PowerBridge::startBatteryMonitoring()
{
    // Register for power setting notifications
    m_powerNotifyHandle = RegisterPowerSettingNotification(
        GetCurrentProcess(),
        &GUID_BATTERY_PERCENTAGE_REMAINING,
        DEVICE_NOTIFY_CALLBACK
    );

    if (m_powerNotifyHandle) {
        LOG_INFO("PowerManager", "Battery monitoring started successfully");
    } else {
        LOG_ERROR("PowerManager", "Failed to register for power notifications");
    }

    // Also register for AC/DC power source changes
    HANDLE acNotifyHandle = RegisterPowerSettingNotification(
        GetCurrentProcess(),
        &GUID_ACDC_POWER_SOURCE,
        DEVICE_NOTIFY_CALLBACK
    );
    
    if (!acNotifyHandle) {
        LOG_ERROR("PowerManager", "Failed to register for AC/DC power notifications");
    }
}

void PowerBridge::stopBatteryMonitoring()
{
    if (m_powerNotifyHandle) {
        UnregisterPowerSettingNotification(m_powerNotifyHandle);
        m_powerNotifyHandle = NULL;
        LOG_INFO("PowerManager", "Battery monitoring stopped");
    }
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