#pragma once
#include <QObject>
#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <headsetcontrol.hpp>
#include <memory>
#include <vector>

struct HeadsetControlDevice {
    QString deviceName;
    QString vendor;
    QString product;
    QString vendorId;
    QString productId;
    QString batteryStatus;  // "BATTERY_AVAILABLE", "BATTERY_CHARGING", "BATTERY_UNAVAILABLE"
    int batteryLevel;       // -1 - 100
    QStringList capabilities;

    HeadsetControlDevice() : batteryLevel(0) {}
};
Q_DECLARE_METATYPE(HeadsetControlDevice)

class HeadsetControlMonitor : public QObject
{
    Q_OBJECT
public:
    explicit HeadsetControlMonitor(QObject *parent = nullptr);
    ~HeadsetControlMonitor();

    bool isMonitoring() const;
    QList<HeadsetControlDevice> getCachedDevices() const { return m_cachedDevices; }

    bool hasSidetoneCapability() const { return m_hasSidetoneCapability; }
    bool hasLightsCapability() const { return m_hasLightsCapability; }
    QString deviceName() const { return m_deviceName; }
    QString batteryStatus() const { return m_batteryStatus; }
    int batteryLevel() const { return m_batteryLevel; }
    bool anyDeviceFound() const { return m_anyDeviceFound; }

public slots:
    void startMonitoring();
    void stopMonitoring();
    void setLights(bool enabled);
    void setSidetone(int value);
    void setFetchInterval(int intervalMs);

signals:
    void headsetDataUpdated(const QList<HeadsetControlDevice>& devices);
    void monitoringStateChanged(bool enabled);
    void capabilitiesChanged();
    void deviceNameChanged();
    void batteryStatusChanged();
    void batteryLevelChanged();
    void anyDeviceFoundChanged();

private slots:
    void fetchHeadsetInfo();

private:
    void updateDeviceCache();
    void updateCapabilities();
    QString batteryStatusToString(battery_status status) const;
    QStringList getCapabilityList(const headsetcontrol::Headset& headset) const;

    QTimer* m_fetchTimer;
    QList<HeadsetControlDevice> m_cachedDevices;
    std::vector<headsetcontrol::Headset> m_headsets;

    bool m_isMonitoring;
    int m_fetchIntervalMs;

    bool m_hasSidetoneCapability;
    bool m_hasLightsCapability;
    QString m_deviceName;
    QString m_batteryStatus;
    int m_batteryLevel;
    bool m_anyDeviceFound;
    bool m_isFetching;
};
