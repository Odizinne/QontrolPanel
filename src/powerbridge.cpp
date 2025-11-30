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
{

}

PowerBridge::~PowerBridge()
{
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
        LogManager::instance()->sendLog(LogManager::PowerManager,
                                        QString("Found %1 user accounts, analyzing...").arg(entriesRead));

        for (DWORD i = 0; i < entriesRead; i++) {
            // Skip built-in system accounts
            QString username = QString::fromWCharArray(buffer[i].usri1_name);
            // Skip common system accounts
            if (username.compare("Administrator", Qt::CaseInsensitive) != 0 &&
                username.compare("Guest", Qt::CaseInsensitive) != 0 &&
                username.compare("DefaultAccount", Qt::CaseInsensitive) != 0 &&
                username.compare("WDAGUtilityAccount", Qt::CaseInsensitive) != 0 &&
                !username.startsWith("_") &&
                !(buffer[i].usri1_flags & UF_ACCOUNTDISABLE)) {
                realUserCount++;
                LogManager::instance()->sendLog(LogManager::PowerManager,
                                                QString("Real user found: %1").arg(username));
            } else {
                LogManager::instance()->sendLog(LogManager::PowerManager,
                                                QString("Skipping system account: %1").arg(username));
            }
        }
        NetApiBufferFree(buffer);
        bool result = realUserCount > 1;
        LogManager::instance()->sendLog(LogManager::PowerManager,
                                        QString("Real users found: %1, hasMultipleUsers returning: %2")
                                            .arg(realUserCount).arg(result ? "true" : "false"));
        return result;
    }

    LogManager::instance()->sendCritical(LogManager::PowerManager,
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

    // Launch elevated shutdown command via ShellExecute
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
