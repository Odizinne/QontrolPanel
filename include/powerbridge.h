#pragma once
#include <QObject>
#include <QQmlEngine>
#include <QtQml/qqmlregistration.h>
#include <QAbstractNativeEventFilter>
#include <windows.h>

class PowerBridge : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(int batteryLevel READ batteryLevel NOTIFY batteryLevelChanged)
    Q_PROPERTY(int batteryStatus READ batteryStatus NOTIFY batteryStatusChanged)
    Q_PROPERTY(bool batterySaverEnabled READ batterySaverEnabled NOTIFY batterySaverEnabledChanged)

public:
    enum BatteryStatus {
        Unknown = 0,
        Discharging = 1,
        Charging = 2,
        NotPresent = 3,
        ACPower = 4
    };
    Q_ENUM(BatteryStatus)

    explicit PowerBridge(QObject* parent = nullptr);
    ~PowerBridge() override;

    static PowerBridge* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);
    static PowerBridge* instance();

    Q_INVOKABLE bool hasMultipleUsers();
    Q_INVOKABLE bool isSleepSupported();
    Q_INVOKABLE bool isHibernateSupported();
    Q_INVOKABLE bool isUEFISupported();
    Q_INVOKABLE bool shutdown();
    Q_INVOKABLE bool restart();
    Q_INVOKABLE bool sleep();
    Q_INVOKABLE bool hibernate();
    Q_INVOKABLE bool lockAccount();
    Q_INVOKABLE bool signOut();
    Q_INVOKABLE bool switchAccount();
    Q_INVOKABLE void restartToUEFI();
    Q_INVOKABLE void setBatterySaver(bool enable);

    int batteryLevel() const { return m_batteryLevel; }
    int batteryStatus() const { return m_batteryStatus; }
    bool batterySaverEnabled() const { return m_batterySaverEnabled; }

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

signals:
    void batteryLevelChanged();
    void batteryStatusChanged();
    void batterySaverEnabledChanged();

private:
    static PowerBridge* m_instance;

    bool enableShutdownPrivilege();
    void updateBatteryStatus();
    void updateBatterySaverStatus();
    void startBatteryMonitoring();
    void stopBatteryMonitoring();

    int m_batteryLevel;
    int m_batteryStatus;
    bool m_batterySaverEnabled;
    HPOWERNOTIFY m_powerNotifyHandle;
    HPOWERNOTIFY m_acNotifyHandle;
    HPOWERNOTIFY m_batterySaverNotifyHandle;
};
