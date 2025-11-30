#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QQmlEngine>
#include <QTimer>

class Updater : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY updateAvailableChanged)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY latestVersionChanged)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(bool isChecking READ isChecking NOTIFY isCheckingChanged)
    Q_PROPERTY(bool isDownloading READ isDownloading NOTIFY isDownloadingChanged)
    Q_PROPERTY(int downloadProgress READ downloadProgress NOTIFY downloadProgressChanged)
    Q_PROPERTY(bool hasReleaseNotes READ hasReleaseNotes NOTIFY hasReleaseNotesChanged)
    Q_PROPERTY(QString releaseNotes READ releaseNotes NOTIFY releaseNotesChanged)

public:
    static Updater* instance();
    static Updater* create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE void checkForAppUpdatesAuto();
    Q_INVOKABLE void downloadAndInstall();
    Q_INVOKABLE void downloadLatestTranslations();
    Q_INVOKABLE void cancelTranslationDownload();
    Q_INVOKABLE int getTranslationProgress(const QString& languageCode);
    Q_INVOKABLE bool hasTranslationProgressData();

    bool updateAvailable() const { return m_updateAvailable; }
    QString latestVersion() const { return m_latestVersion; }
    QString currentVersion() const { return getCurrentVersion(); }
    bool isChecking() const { return m_isChecking; }
    bool isDownloading() const { return m_isDownloading; }
    int downloadProgress() const { return m_downloadProgress; }
    bool hasReleaseNotes() const { return !m_releaseNotes.isEmpty(); }
    QString releaseNotes() const { return m_releaseNotes; }

signals:
    void updateAvailableChanged();
    void latestVersionChanged();
    void isCheckingChanged();
    void isDownloadingChanged();
    void downloadProgressChanged();
    void hasReleaseNotesChanged();
    void releaseNotesChanged();
    void updateFinished(bool success, const QString& message);
    void updateAvailableNotification(const QString& version);
    void translationDownloadStarted();
    void translationDownloadProgress(const QString& language, int bytesReceived, int bytesTotal);
    void translationDownloadFinished(bool success, const QString& message);
    void translationDownloadError(const QString& errorMessage);
    void translationFileCompleted(const QString& language, int completed, int total);
    void translationProgressDataLoaded();

private slots:
    void onVersionCheckFinished();
    void onDownloadFinished();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onTranslationFileDownloaded();
    void checkForTranslationUpdates();
    void checkForAppUpdatesTimer();

private:
    explicit Updater(QObject *parent = nullptr);
    static Updater* m_instance;

    QNetworkAccessManager* m_networkManager;
    bool m_updateAvailable;
    bool m_isChecking;
    bool m_isDownloading;
    int m_downloadProgress;
    QString m_latestVersion;
    QString m_downloadUrl;
    QString m_releaseNotes;

    QList<QNetworkReply*> m_activeTranslationDownloads;
    int m_totalTranslationDownloads;
    int m_completedTranslationDownloads;
    int m_failedTranslationDownloads;
    QJsonObject m_translationProgress;
    QTimer* m_translationAutoUpdateTimer;
    QTimer* m_appUpdateCheckTimer;

    QString getCurrentVersion() const;
    bool isNewerVersion(const QString& latest, const QString& current) const;
    void installExecutable(const QString& newExePath);
    void setChecking(bool checking);
    void setDownloading(bool downloading);
    void setDownloadProgress(int progress);
    void setReleaseNotes(const QString& notes);
    void downloadTranslationFile(const QString& languageCode, const QString& githubUrl);
    QString getTranslationDownloadPath() const;
    QString getTranslationProgressPath() const;
    void loadTranslationProgressData();
    void downloadTranslationProgressFile();
};
