#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QtQml/qqmlregistration.h>

class PowerBridge : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
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

private:
    static PowerBridge* m_instance;

    bool enableShutdownPrivilege();
};

