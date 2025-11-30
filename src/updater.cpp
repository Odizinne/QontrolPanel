#include "updater.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>
#include <QDebug>
#include <QSettings>
#include "version.h"
#include "logmanager.h"
#include "languages.h"

Updater* Updater::m_instance = nullptr;

Updater* Updater::instance()
{
    if (!m_instance) {
        m_instance = new Updater();
    }
    return m_instance;
}

Updater* Updater::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    return instance();
}

Updater::Updater(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_updateAvailable(false)
    , m_isChecking(false)
    , m_isDownloading(false)
    , m_downloadProgress(0)
    , m_totalTranslationDownloads(0)
    , m_completedTranslationDownloads(0)
    , m_failedTranslationDownloads(0)
    , m_translationAutoUpdateTimer(new QTimer(this))
    , m_appUpdateCheckTimer(new QTimer(this))
{
    loadTranslationProgressData();

    m_translationAutoUpdateTimer->setInterval(4 * 60 * 60 * 1000);
    m_translationAutoUpdateTimer->setSingleShot(false);
    connect(m_translationAutoUpdateTimer, &QTimer::timeout, this, &Updater::checkForTranslationUpdates);
    m_translationAutoUpdateTimer->start();

    m_appUpdateCheckTimer->setInterval(4 * 60 * 60 * 1000);
    m_appUpdateCheckTimer->setSingleShot(false);
    connect(m_appUpdateCheckTimer, &QTimer::timeout, this, &Updater::checkForAppUpdatesTimer);
    m_appUpdateCheckTimer->start();

    QSettings settings("Odizinne", "QontrolPanel");
    if (settings.value("autoUpdateTranslations", false).toBool()) {
        QTimer::singleShot(5000, this, [this]() {
            downloadLatestTranslations();
        });
    }

    if (settings.value("autoFetchForAppUpdates", false).toBool()) {
        QTimer::singleShot(5000, this, [this]() {
            checkForAppUpdatesAuto();
        });
    }
}

void Updater::checkForUpdates()
{
    if (m_isChecking || m_isDownloading) {
        return;
    }

    setChecking(true);

    // Clear previous release notes
    setReleaseNotes("");

    QString urlString = "https://api.github.com/repos/Odizinne/QontrolPanel/releases/latest";
    QUrl url{urlString};
    QNetworkRequest request{url};
    request.setHeader(QNetworkRequest::UserAgentHeader, "QontrolPanel-Updater");

    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &Updater::onVersionCheckFinished);
}

void Updater::onVersionCheckFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    reply->deleteLater();

    setChecking(false);

    if (reply->error() != QNetworkReply::NoError) {
        LogManager::instance()->sendCritical(LogManager::Updater,
                                             QString("Update check failed: %1").arg(reply->errorString()));
        emit updateFinished(false, "Failed to check for updates: " + reply->errorString());
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject obj = doc.object();

    m_latestVersion = obj["tag_name"].toString();
    if (m_latestVersion.startsWith("v")) {
        m_latestVersion = m_latestVersion.mid(1);
    }

    // Extract release notes
    QString releaseBody = obj["body"].toString();
    if (!releaseBody.isEmpty()) {
        setReleaseNotes(releaseBody);
    }

    // Find the .exe asset
    QJsonArray assets = obj["assets"].toArray();
    m_downloadUrl.clear();

    for (const auto& asset : assets) {
        QJsonObject assetObj = asset.toObject();
        QString name = assetObj["name"].toString();
        if (name.endsWith(".exe") && name.contains("QontrolPanel")) {
            m_downloadUrl = assetObj["browser_download_url"].toString();
            break;
        }
    }

    if (m_downloadUrl.isEmpty()) {
        emit updateFinished(false, "No executable found in latest release");
        return;
    }

    QString currentVersion = getCurrentVersion();
    bool wasUpdateAvailable = m_updateAvailable;
    m_updateAvailable = isNewerVersion(m_latestVersion, currentVersion);

    if (wasUpdateAvailable != m_updateAvailable) {
        emit updateAvailableChanged();
    }
    emit latestVersionChanged();

    if (m_updateAvailable) {
        emit updateFinished(true, tr("Update available: ") + m_latestVersion);
    } else {
        emit updateFinished(true, tr("You are using the latest version"));
    }
}

void Updater::downloadAndInstall()
{
    if (m_downloadUrl.isEmpty() || m_isDownloading || m_isChecking) {
        emit updateFinished(false, "Cannot start download");
        return;
    }

    setDownloading(true);
    setDownloadProgress(0);

    QUrl url{m_downloadUrl};
    QNetworkRequest request{url};
    QNetworkReply* reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::downloadProgress, this, &Updater::onDownloadProgress);
    connect(reply, &QNetworkReply::finished, this, &Updater::onDownloadFinished);
}

void Updater::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0) {
        int percentage = static_cast<int>((bytesReceived * 100) / bytesTotal);
        setDownloadProgress(percentage);
    }
}

void Updater::onDownloadFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    reply->deleteLater();

    setDownloading(false);
    setDownloadProgress(0);

    if (reply->error() != QNetworkReply::NoError) {
        emit updateFinished(false, "Download failed: " + reply->errorString());
        return;
    }

    // Save to temp file
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString tempFile = tempDir + "/QontrolPanel_update.exe";

    QFile file(tempFile);
    if (!file.open(QIODevice::WriteOnly)) {
        emit updateFinished(false, "Failed to save update file");
        return;
    }

    file.write(reply->readAll());
    file.close();

    installExecutable(tempFile);
}

void Updater::installExecutable(const QString& newExePath)
{
    QString currentExeDir = QApplication::applicationDirPath();
    QDir dir(currentExeDir);
    dir.cdUp();
    dir.cdUp();
    QString targetDir = QDir::toNativeSeparators(dir.absolutePath());

    bool started = QProcess::startDetached(newExePath);

    if (started) {
        emit updateFinished(true, tr("Update started."));
        QApplication::quit();
    } else {
        emit updateFinished(false, "Failed to start update executable");
    }
}

QString Updater::getCurrentVersion() const
{
    return QString(APP_VERSION_STRING);
}

bool Updater::isNewerVersion(const QString& latest, const QString& current) const
{
    QStringList latestParts = latest.split('.');
    QStringList currentParts = current.split('.');

    // Pad with zeros if needed
    while (latestParts.size() < 3) latestParts.append("0");
    while (currentParts.size() < 3) currentParts.append("0");

    for (int i = 0; i < 3; ++i) {
        int latestNum = latestParts[i].toInt();
        int currentNum = currentParts[i].toInt();

        if (latestNum > currentNum) return true;
        if (latestNum < currentNum) return false;
    }

    return false; // Versions are equal
}

void Updater::setChecking(bool checking)
{
    if (m_isChecking != checking) {
        m_isChecking = checking;
        emit isCheckingChanged();
    }
}

void Updater::setDownloading(bool downloading)
{
    if (m_isDownloading != downloading) {
        m_isDownloading = downloading;
        emit isDownloadingChanged();
    }
}

void Updater::setDownloadProgress(int progress)
{
    if (m_downloadProgress != progress) {
        m_downloadProgress = progress;
        emit downloadProgressChanged();
    }
}

void Updater::setReleaseNotes(const QString& notes)
{
    if (m_releaseNotes != notes) {
        bool hadNotes = !m_releaseNotes.isEmpty();
        m_releaseNotes = notes;
        bool hasNotes = !m_releaseNotes.isEmpty();

        emit releaseNotesChanged();

        if (hadNotes != hasNotes) {
            emit hasReleaseNotesChanged();
        }
    }
}

void Updater::checkForTranslationUpdates()
{
    QSettings settings("Odizinne", "QontrolPanel");
    if (!settings.value("autoUpdateTranslations", false).toBool()) {
        m_translationAutoUpdateTimer->stop();
        return;
    }

    if (m_activeTranslationDownloads.isEmpty()) {
        downloadLatestTranslations();
    }
}

void Updater::downloadLatestTranslations()
{
    cancelTranslationDownload();

    m_totalTranslationDownloads = 0;
    m_completedTranslationDownloads = 0;
    m_failedTranslationDownloads = 0;

    QStringList languageCodes = getLanguageCodes();
    QString baseUrl = "https://github.com/Odizinne/QontrolPanel/raw/refs/heads/main/i18n/compiled/QontrolPanel_%1.qm";

    m_totalTranslationDownloads = languageCodes.size();
    emit translationDownloadStarted();

    for (const QString& langCode : languageCodes) {
        QString url = baseUrl.arg(langCode);
        downloadTranslationFile(langCode, url);
    }

    downloadTranslationProgressFile();

    if (m_totalTranslationDownloads == 0) {
        emit translationDownloadFinished(false, tr("No translation files to download"));
    }
}

void Updater::cancelTranslationDownload()
{
    for (QNetworkReply* reply : m_activeTranslationDownloads) {
        if (reply && reply->isRunning()) {
            reply->abort();
        }
    }
    m_activeTranslationDownloads.clear();
}

void Updater::downloadTranslationFile(const QString& languageCode, const QString& githubUrl)
{
    QUrl url(githubUrl);
    QNetworkRequest request;
    request.setUrl(url);

    QString userAgent = QString("QontrolPanel/%1").arg(APP_VERSION_STRING);
    request.setRawHeader("User-Agent", userAgent.toUtf8());
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    request.setRawHeader("Accept", "*/*");
    request.setRawHeader("Connection", "keep-alive");
    request.setTransferTimeout(30000);

    QNetworkReply* reply = m_networkManager->get(request);
    reply->setProperty("languageCode", languageCode);
    m_activeTranslationDownloads.append(reply);

    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, languageCode](qint64 bytesReceived, qint64 bytesTotal) {
                emit translationDownloadProgress(languageCode,
                                                 static_cast<int>(bytesReceived),
                                                 static_cast<int>(bytesTotal));
            });

    connect(reply, &QNetworkReply::finished, this, &Updater::onTranslationFileDownloaded);

    connect(reply, &QNetworkReply::errorOccurred, this,
            [this, languageCode](QNetworkReply::NetworkError error) {
                qWarning() << "Network error for" << languageCode << ":" << error;
            });
}

void Updater::onTranslationFileDownloaded()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    QString languageCode = reply->property("languageCode").toString();
    m_activeTranslationDownloads.removeAll(reply);

    if (reply->error() == QNetworkReply::NoError) {
        QString downloadPath = getTranslationDownloadPath();
        QString fileName = QString("QontrolPanel_%1.qm").arg(languageCode);
        QString filePath = downloadPath + "/" + fileName;

        QDir().mkpath(downloadPath);

        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly)) {
            QByteArray data = reply->readAll();
            if (!data.isEmpty()) {
                file.write(data);
                file.close();
            } else {
                qWarning() << "Downloaded empty file for:" << languageCode;
                m_failedTranslationDownloads++;
            }
        } else {
            qWarning() << "Failed to save translation file:" << filePath;
            m_failedTranslationDownloads++;
        }
    } else {
        qWarning() << "Failed to download translation for" << languageCode
                   << "Error:" << reply->error()
                   << "HTTP Status:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()
                   << "Message:" << reply->errorString();
        m_failedTranslationDownloads++;
    }

    m_completedTranslationDownloads++;
    emit translationFileCompleted(languageCode, m_completedTranslationDownloads, m_totalTranslationDownloads);

    if (m_completedTranslationDownloads >= m_totalTranslationDownloads) {
        bool success = (m_failedTranslationDownloads == 0);
        QString message;

        if (success) {
            message = tr("All translations downloaded successfully");
        } else {
            message = tr("Downloaded %1 of %2 translation files")
            .arg(m_totalTranslationDownloads - m_failedTranslationDownloads)
                .arg(m_totalTranslationDownloads);
        }

        emit translationDownloadFinished(success, message);
    }

    reply->deleteLater();
}

QString Updater::getTranslationDownloadPath() const
{
    QString appDir = QCoreApplication::applicationDirPath();
    return appDir + "/i18n";
}

QString Updater::getTranslationProgressPath() const
{
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appDataPath);
    return appDataPath + "/translation_progress.json";
}

void Updater::loadTranslationProgressData()
{
    QString progressFilePath = getTranslationProgressPath();
    LogManager::instance()->sendLog(LogManager::Updater,
                                    QString("Loading translation progress data from: %1").arg(progressFilePath));

    QFile file(progressFilePath);
    if (!file.exists()) {
        LogManager::instance()->sendWarn(LogManager::Updater,
                                         QString("Translation progress file does not exist: %1").arg(progressFilePath));
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        LogManager::instance()->sendCritical(LogManager::Updater,
                                             QString("Failed to open translation progress file: %1").arg(file.errorString()));
        return;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        LogManager::instance()->sendCritical(LogManager::Updater,
                                             "Failed to parse translation progress JSON - invalid format");
        return;
    }

    m_translationProgress = doc.object();
    LogManager::instance()->sendLog(LogManager::Updater,
                                    "Translation progress data loaded successfully");
    emit translationProgressDataLoaded();
}

void Updater::downloadTranslationProgressFile()
{
    QString githubUrl = "https://raw.githubusercontent.com/Odizinne/QontrolPanel/main/i18n/compiled/translation_progress.json";
    LogManager::instance()->sendLog(LogManager::Updater,
                                    QString("Downloading translation progress file from: %1").arg(githubUrl));

    QNetworkRequest request(githubUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader, "QontrolPanel");
    QNetworkReply* reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::finished, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            LogManager::instance()->sendLog(LogManager::Updater,
                                            QString("Translation progress data downloaded (%1 bytes)").arg(data.size()));

            QString progressFilePath = getTranslationProgressPath();
            QFile file(progressFilePath);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(data);
                file.close();
                LogManager::instance()->sendLog(LogManager::Updater,
                                                "Translation progress file saved successfully");
                loadTranslationProgressData();
            } else {
                LogManager::instance()->sendCritical(LogManager::Updater,
                                                     QString("Failed to save translation progress file: %1").arg(file.errorString()));
            }
        } else {
            LogManager::instance()->sendCritical(LogManager::Updater,
                                                 QString("Failed to download translation progress: %1").arg(reply->errorString()));
        }
        reply->deleteLater();
    });
}

int Updater::getTranslationProgress(const QString& languageCode)
{
    if (m_translationProgress.contains(languageCode)) {
        return m_translationProgress[languageCode].toInt();
    }
    return 0;
}

bool Updater::hasTranslationProgressData()
{
    return !m_translationProgress.isEmpty();
}

void Updater::checkForAppUpdatesTimer()
{
    QSettings settings("Odizinne", "QontrolPanel");
    if (!settings.value("autoFetchForAppUpdates", false).toBool()) {
        m_appUpdateCheckTimer->stop();
        return;
    }

    checkForAppUpdatesAuto();
}

void Updater::checkForAppUpdatesAuto()
{
    QSettings settings("Odizinne", "QontrolPanel");
    if (!settings.value("autoFetchForAppUpdates", false).toBool()) {
        return;
    }

    if (m_isChecking || m_isDownloading) {
        return;
    }

    connect(this, &Updater::updateFinished, this,
            [this](bool success, const QString& message) {
                disconnect(this, &Updater::updateFinished, this, nullptr);

                if (success && m_updateAvailable) {
                    emit updateAvailableNotification(m_latestVersion);
                }
            }, Qt::SingleShotConnection);

    checkForUpdates();
}
