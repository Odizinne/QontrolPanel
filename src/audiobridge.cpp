#include "audiobridge.h"
#include "audiomanager.h"
#include "audiomodels.h"
#include "usersettings.h"
#include "keyboardshortcutmanager.h"
#include "logmanager.h"
#include <QDebug>
#include <QQmlContext>

AudioBridge* AudioBridge::m_instance = nullptr;

AudioBridge* AudioBridge::instance()
{
    if (!m_instance) {
        m_instance = new AudioBridge();
    }
    return m_instance;
}

AudioBridge::AudioBridge(QObject *parent)
    : QObject(parent)
    , m_outputVolume(0)
    , m_inputVolume(0)
    , m_outputMuted(false)
    , m_inputMuted(false)
    , m_isReady(false)
    , m_applicationModel(new ApplicationModel(this))
    , m_groupedApplicationModel(new GroupedApplicationModel(this))
    , m_outputDeviceModel(new FilteredDeviceModel(false, this))
    , m_inputDeviceModel(new FilteredDeviceModel(true, this))
    , m_outputDeviceDisplayName("")
    , m_inputDeviceDisplayName("")
{
    auto* manager = AudioManager::instance();

    // Connect volume signals
    connect(manager, &AudioManager::outputVolumeChanged, this, &AudioBridge::onOutputVolumeChanged);
    connect(manager, &AudioManager::inputVolumeChanged, this, &AudioBridge::onInputVolumeChanged);
    connect(manager, &AudioManager::outputMuteChanged, this, &AudioBridge::onOutputMuteChanged);
    connect(manager, &AudioManager::inputMuteChanged, this, &AudioBridge::onInputMuteChanged);

    // Connect application signals
    connect(manager, &AudioManager::applicationsChanged, this, &AudioBridge::onApplicationsChanged);
    connect(manager, &AudioManager::applicationVolumeChanged, this, &AudioBridge::onApplicationVolumeChanged);
    connect(manager, &AudioManager::applicationMuteChanged, this, &AudioBridge::onApplicationMuteChanged);

    // Connect device signals
    connect(manager, &AudioManager::devicesChanged, this, &AudioBridge::onDevicesChanged);
    connect(manager, &AudioManager::deviceAdded, this, &AudioBridge::onDeviceAdded);
    connect(manager, &AudioManager::deviceRemoved, this, &AudioBridge::onDeviceRemoved);
    connect(manager, &AudioManager::defaultDeviceChanged, this, &AudioBridge::onDefaultDeviceChanged);
    connect(manager, &AudioManager::initializationComplete, this, &AudioBridge::onInitializationComplete);
    connect(manager, &AudioManager::outputAudioLevelChanged, this, &AudioBridge::onOutputAudioLevelChanged);
    connect(manager, &AudioManager::inputAudioLevelChanged, this, &AudioBridge::onInputAudioLevelChanged);

    connect(manager, &AudioManager::applicationAudioLevelChanged,
            this, &AudioBridge::onApplicationAudioLevelChanged);

    m_windowFocusManager = new WindowFocusManager(this);
    connect(m_windowFocusManager, &WindowFocusManager::applicationFocusChanged,
            this, &AudioBridge::onApplicationFocusChanged);
    m_windowFocusManager->startMonitoring();

    // Connect per-app volume hotkeys
    connect(KeyboardShortcutManager::instance(), &KeyboardShortcutManager::appVolumeHotkeyPressed,
            this, &AudioBridge::onAppVolumeHotkeyPressed);

    loadCommAppsFromFile();
    loadAppRenamesFromFile();
    loadExecutableRenamesFromFile();
    loadAppLocksFromFile();
    loadDeviceRenamesFromFile();
    loadDeviceIconsFromFile();

    bool enableDeviceManager = UserSettings::instance()->enableDeviceManager();
    bool enableApplicationMixer = UserSettings::instance()->enableApplicationMixer();

    if (enableDeviceManager || enableApplicationMixer) {
        manager->initialize();
    } else {
        LOG_INFO("AudioManager", "Audio components disabled in settings, skipping initialization");
    }
}

AudioBridge::~AudioBridge() {
    bool activateChatMix = UserSettings::instance()->activateChatmix();
    bool chatMixEnabled = UserSettings::instance()->chatMixEnabled();

    if (activateChatMix && chatMixEnabled) {
        restoreOriginalVolumesSync();
    }

    // Clean up session models
    for (auto* model : m_sessionModels) {
        delete model;
    }
    m_sessionModels.clear();

    auto* manager = AudioManager::instance();
    manager->cleanup();
}

AudioBridge* AudioBridge::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    return instance();
}

// Grouped application methods
ExecutableSessionModel* AudioBridge::getSessionsForExecutable(const QString& executableName)
{
    if (!m_sessionModels.contains(executableName)) {
        m_sessionModels[executableName] = new ExecutableSessionModel(this);
    }

    ExecutableSessionModel* model = m_sessionModels[executableName];

    // Find sessions for this executable
    QList<AudioApplication> sessions;
    for (int i = 0; i < m_applicationModel->rowCount(); ++i) {
        QModelIndex index = m_applicationModel->index(i, 0);
        QString appExecutableName = m_applicationModel->data(index, ApplicationModel::ExecutableNameRole).toString();

        if (appExecutableName == executableName) {
            AudioApplication app;
            app.id = m_applicationModel->data(index, ApplicationModel::IdRole).toString();
            app.executableName = appExecutableName;
            app.iconPath = m_applicationModel->data(index, ApplicationModel::IconPathRole).toString();
            app.volume = m_applicationModel->data(index, ApplicationModel::VolumeRole).toInt();
            app.isMuted = m_applicationModel->data(index, ApplicationModel::IsMutedRole).toBool();
            app.streamIndex = m_applicationModel->data(index, ApplicationModel::StreamIndexRole).toInt();

            // Use the original name first, then get the display name (which includes custom names)
            QString originalName = m_applicationModel->data(index, ApplicationModel::NameRole).toString();
            app.name = getDisplayNameForApplication(originalName, app.streamIndex);

            sessions.append(app);
        }
    }

    // Sort sessions by stream index to maintain consistent order
    std::sort(sessions.begin(), sessions.end(),
              [](const AudioApplication& a, const AudioApplication& b) {
                  return a.streamIndex < b.streamIndex;
              });

    model->setSessions(sessions);
    return model;
}

void AudioBridge::setExecutableVolume(const QString& executableName, int volume)
{
    // Temporarily disable individual update signals to avoid loops
    bool wasBlocked = blockSignals(true);

    // Set volume for all UNLOCKED sessions of this executable
    for (int i = 0; i < m_applicationModel->rowCount(); ++i) {
        QModelIndex index = m_applicationModel->index(i, 0);
        QString appExecutableName = m_applicationModel->data(index, ApplicationModel::ExecutableNameRole).toString();

        if (appExecutableName == executableName) {
            QString appId = m_applicationModel->data(index, ApplicationModel::IdRole).toString();
            QString originalName = m_applicationModel->data(index, ApplicationModel::NameRole).toString();
            int streamIndex = m_applicationModel->data(index, ApplicationModel::StreamIndexRole).toInt();

            // Only set volume if this stream is not locked
            if (!isApplicationLocked(originalName, streamIndex)) {
                setApplicationVolume(appId, volume);
            }
        }
    }

    blockSignals(wasBlocked);

    // Update the grouped model to show the volume we just set (for unlocked streams)
    // This way the slider shows the volume that will be applied to unlocked streams
    m_groupedApplicationModel->updateGroupVolume(executableName, volume);

    // Update session model with actual current volumes
    if (m_sessionModels.contains(executableName)) {
        for (int i = 0; i < m_applicationModel->rowCount(); ++i) {
            QModelIndex index = m_applicationModel->index(i, 0);
            QString appExecutableName = m_applicationModel->data(index, ApplicationModel::ExecutableNameRole).toString();

            if (appExecutableName == executableName) {
                QString appId = m_applicationModel->data(index, ApplicationModel::IdRole).toString();
                int currentVolume = m_applicationModel->data(index, ApplicationModel::VolumeRole).toInt();
                m_sessionModels[executableName]->updateSessionVolume(appId, currentVolume);
            }
        }
    }
}

int AudioBridge::getExecutableVolume(const QString& executableName) const
{
    int totalVolume = 0;
    int count = 0;
    for (int i = 0; i < m_applicationModel->rowCount(); ++i) {
        QModelIndex index = m_applicationModel->index(i, 0);
        QString appExeName = m_applicationModel->data(index, ApplicationModel::ExecutableNameRole).toString();
        if (appExeName == executableName) {
            totalVolume += m_applicationModel->data(index, ApplicationModel::VolumeRole).toInt();
            count++;
        }
    }
    return count > 0 ? totalVolume / count : -1;
}

void AudioBridge::onAppVolumeHotkeyPressed(const QString &executableName, bool volumeUp, int volumeStepSize)
{
    int currentVolume = getExecutableVolume(executableName);
    if (currentVolume < 0) return;

    int step = (volumeStepSize > 0) ? volumeStepSize : UserSettings::instance()->sliderWheelSensivity();
    int newVolume = volumeUp ? qMin(100, currentVolume + step) : qMax(0, currentVolume - step);
    setExecutableVolume(executableName, newVolume);
}

void AudioBridge::setExecutableMute(const QString& executableName, bool muted)
{
    // Set mute for all sessions of this executable
    for (int i = 0; i < m_applicationModel->rowCount(); ++i) {
        QModelIndex index = m_applicationModel->index(i, 0);
        QString appExecutableName = m_applicationModel->data(index, ApplicationModel::ExecutableNameRole).toString();

        if (appExecutableName == executableName) {
            QString appId = m_applicationModel->data(index, ApplicationModel::IdRole).toString();
            setApplicationMute(appId, muted);
        }
    }

    // Update grouped model in place
    m_groupedApplicationModel->updateGroupMute(executableName, muted, muted);

    // Update session model
    if (m_sessionModels.contains(executableName)) {
        for (int i = 0; i < m_applicationModel->rowCount(); ++i) {
            QModelIndex index = m_applicationModel->index(i, 0);
            QString appExecutableName = m_applicationModel->data(index, ApplicationModel::ExecutableNameRole).toString();

            if (appExecutableName == executableName) {
                QString appId = m_applicationModel->data(index, ApplicationModel::IdRole).toString();
                m_sessionModels[executableName]->updateSessionMute(appId, muted);
            }
        }
    }
}

void AudioBridge::updateGroupedApplications()
{
    QMap<QString, ApplicationGroup> groups;

    for (int i = 0; i < m_applicationModel->rowCount(); ++i) {
        QModelIndex index = m_applicationModel->index(i, 0);

        QString executableName = m_applicationModel->data(index, ApplicationModel::ExecutableNameRole).toString();
        QString name = m_applicationModel->data(index, ApplicationModel::NameRole).toString();
        QString iconPath = m_applicationModel->data(index, ApplicationModel::IconPathRole).toString();
        int volume = m_applicationModel->data(index, ApplicationModel::VolumeRole).toInt();
        bool muted = m_applicationModel->data(index, ApplicationModel::IsMutedRole).toBool();
        QString appId = m_applicationModel->data(index, ApplicationModel::IdRole).toString();
        int streamIndex = m_applicationModel->data(index, ApplicationModel::StreamIndexRole).toInt();

        AudioApplication app;
        app.id = appId;
        app.name = name;
        app.executableName = executableName;
        app.iconPath = iconPath;
        app.volume = volume;
        app.isMuted = muted;
        app.streamIndex = streamIndex;

        if (!groups.contains(executableName)) {
            ApplicationGroup group;
            group.executableName = executableName;

            // Use custom name if available, otherwise try to get the proper display name
            QString customName = getCustomExecutableName(executableName);
            if (customName == executableName) {
                // No custom name set, trying to get proper display name
                QString properDisplayName = getDisplayNameForApplication(name, streamIndex);
                group.displayName = properDisplayName;
            } else {
                group.displayName = customName;
            }

            group.iconPath = iconPath;
            group.sessions.append(app);
            groups[executableName] = group;
        } else {
            groups[executableName].sessions.append(app);
        }
    }

    // Calculate group statistics
    QList<ApplicationGroup> groupList;
    for (auto& group : groups) {
        int totalVolume = 0;
        int mutedCount = 0;

        for (const auto& app : group.sessions) {
            totalVolume += app.volume;
            if (app.isMuted) mutedCount++;
        }

        group.averageVolume = group.sessions.isEmpty() ? 0 : totalVolume / group.sessions.count();
        group.anyMuted = mutedCount > 0;
        group.allMuted = mutedCount == group.sessions.count();
        group.sessionCount = group.sessions.count();

        groupList.append(group);
    }

    // Sort groups (System sounds last, others alphabetically)
    std::sort(groupList.begin(), groupList.end(),
              [](const ApplicationGroup& a, const ApplicationGroup& b) {
                  if (a.executableName == "System sounds") return false;
                  if (b.executableName == "System sounds") return true;
                  return a.displayName.toLower() < b.displayName.toLower();
              });

    m_groupedApplicationModel->setGroups(groupList);

    // Update session models for each executable
    for (const auto& group : groupList) {
        if (m_sessionModels.contains(group.executableName)) {
            m_sessionModels[group.executableName]->setSessions(group.sessions);
        }
    }
}

// Volume control methods (unchanged)
void AudioBridge::setOutputVolume(int volume)
{
    AudioManager::instance()->setOutputVolumeAsync(volume);
}

void AudioBridge::setInputVolume(int volume)
{
    AudioManager::instance()->setInputVolumeAsync(volume);
}

void AudioBridge::setOutputMute(bool mute)
{
    AudioManager::instance()->setOutputMuteAsync(mute);
}

void AudioBridge::setInputMute(bool mute)
{
    AudioManager::instance()->setInputMuteAsync(mute);
}

void AudioBridge::setApplicationVolume(const QString& appId, int volume)
{
    AudioManager::instance()->setApplicationVolumeAsync(appId, volume);
}

void AudioBridge::setApplicationMute(const QString& appId, bool mute)
{
    AudioManager::instance()->setApplicationMuteAsync(appId, mute);
}

// Device management methods (unchanged)
void AudioBridge::setDefaultDevice(const QString& deviceId, bool isInput, bool forCommunications)
{
    AudioManager::instance()->setDefaultDeviceAsync(deviceId, isInput, forCommunications);
}

void AudioBridge::setOutputDevice(int deviceIndex)
{
    if (deviceIndex >= 0 && deviceIndex < m_outputDeviceModel->rowCount()) {
        QModelIndex modelIndex = m_outputDeviceModel->index(deviceIndex);
        QString deviceId = m_outputDeviceModel->data(modelIndex, FilteredDeviceModel::IdRole).toString();

        setDefaultDevice(deviceId, false, false);
        setDefaultDevice(deviceId, false, true);
    }
}

void AudioBridge::setInputDevice(int deviceIndex)
{
    if (deviceIndex >= 0 && deviceIndex < m_inputDeviceModel->rowCount()) {
        QModelIndex modelIndex = m_inputDeviceModel->index(deviceIndex);
        QString deviceId = m_inputDeviceModel->data(modelIndex, FilteredDeviceModel::IdRole).toString();

        setDefaultDevice(deviceId, true, false);
        setDefaultDevice(deviceId, true, true);
    }
}

void AudioBridge::applyChatMixToApplications(int value)
{
    for (int i = 0; i < m_applicationModel->rowCount(); ++i) {
        QModelIndex index = m_applicationModel->index(i, 0);
        QString appId = m_applicationModel->data(index, ApplicationModel::IdRole).toString();
        QString appName = m_applicationModel->data(index, ApplicationModel::NameRole).toString();
        int streamIndex = m_applicationModel->data(index, ApplicationModel::StreamIndexRole).toInt();

        if (appName == "System sounds" || appId == "system_sounds") {
            continue;
        }

        // Skip locked applications
        if (isApplicationLocked(appName, streamIndex)) {
            continue;
        }

        int targetVolume = isCommApp(appName) ? 100 : value;
        setApplicationVolume(appId, targetVolume);
    }
}

void AudioBridge::restoreOriginalVolumes()
{
    int restoreVolume = UserSettings::instance()->chatmixRestoreVolume();

    if (!m_isReady) {
        return;
    }

    for (int i = 0; i < m_applicationModel->rowCount(); ++i) {
        QModelIndex index = m_applicationModel->index(i, 0);
        QString appId = m_applicationModel->data(index, ApplicationModel::IdRole).toString();
        QString appName = m_applicationModel->data(index, ApplicationModel::NameRole).toString();
        int streamIndex = m_applicationModel->data(index, ApplicationModel::StreamIndexRole).toInt();

        if (appName == "System sounds" || appId == "system_sounds") {
            continue;
        }

        // Skip locked applications
        if (isApplicationLocked(appName, streamIndex)) {
            continue;
        }

        setApplicationVolume(appId, restoreVolume);
    }
}

void AudioBridge::applyChatMixIfEnabled()
{
    bool activateChatMix = UserSettings::instance()->activateChatmix();
    bool chatMixEnabled = UserSettings::instance()->chatMixEnabled();

    if (activateChatMix && chatMixEnabled) {
        applyChatMixToApplications(UserSettings::instance()->chatMixValue());
    }
}

bool AudioBridge::isCommApp(const QString& name) const
{
    for (const CommApp& app : m_commApps) {
        if (app.name.compare(name, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

void AudioBridge::addCommApp(const QString& name)
{
    for (const CommApp& existing : m_commApps) {
        if (existing.name.compare(name, Qt::CaseInsensitive) == 0) {
            return;
        }
    }

    CommApp newApp;
    newApp.name = name;

    for (int i = 0; i < m_applicationModel->rowCount(); ++i) {
        QModelIndex index = m_applicationModel->index(i, 0);
        QString appName = m_applicationModel->data(index, ApplicationModel::NameRole).toString();

        if (appName.compare(name, Qt::CaseInsensitive) == 0) {
            newApp.icon = m_applicationModel->data(index, ApplicationModel::IconPathRole).toString();
            break;
        }
    }

    m_commApps.append(newApp);
    saveCommAppsToFile();
    emit commAppsListChanged();
}

void AudioBridge::removeCommApp(const QString& name)
{
    for (int i = 0; i < m_commApps.count(); ++i) {
        if (m_commApps[i].name.compare(name, Qt::CaseInsensitive) == 0) {
            m_commApps.removeAt(i);
            saveCommAppsToFile();
            emit commAppsListChanged();
            return;
        }
    }
}

QVariantList AudioBridge::commAppsList() const
{
    QVariantList result;
    for (const CommApp& app : m_commApps) {
        QVariantMap appMap;
        appMap["name"] = app.name;
        appMap["icon"] = app.icon;
        result.append(appMap);
    }
    return result;
}

QString AudioBridge::getCommAppsFilePath() const
{
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appDataPath);
    return appDataPath + "/commapps.json";
}

void AudioBridge::loadCommAppsFromFile()
{
    QString filePath = getCommAppsFilePath();
    QFile file(filePath);

    if (!file.exists()) {
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    QByteArray data = file.readAll();
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError) {
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray commAppsArray = root["commApps"].toArray();

    m_commApps.clear();
    for (const QJsonValue& value : commAppsArray) {
        QJsonObject appObj = value.toObject();
        CommApp app;
        app.name = appObj["name"].toString();
        app.icon = appObj["icon"].toString();
        m_commApps.append(app);
    }
}

void AudioBridge::saveCommAppsToFile()
{
    QString filePath = getCommAppsFilePath();
    QFile file(filePath);

    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }

    QJsonArray commAppsArray;
    for (const CommApp& app : m_commApps) {
        QJsonObject appObj;
        appObj["name"] = app.name;
        appObj["icon"] = app.icon;
        commAppsArray.append(appObj);
    }

    QJsonObject root;
    root["commApps"] = commAppsArray;

    QJsonDocument doc(root);
    file.write(doc.toJson());
}

// Event handlers
void AudioBridge::onOutputVolumeChanged(int volume)
{
    if (m_outputVolume != volume) {
        m_outputVolume = volume;
        emit outputVolumeChanged();
    }
}

void AudioBridge::onInputVolumeChanged(int volume)
{
    if (m_inputVolume != volume) {
        m_inputVolume = volume;
        emit inputVolumeChanged();
    }
}

void AudioBridge::onOutputMuteChanged(bool muted)
{
    if (m_outputMuted != muted) {
        m_outputMuted = muted;
        emit outputMutedChanged();
    }
}

void AudioBridge::onInputMuteChanged(bool muted)
{
    if (m_inputMuted != muted) {
        m_inputMuted = muted;
        emit inputMutedChanged();
    }
}

void AudioBridge::onApplicationVolumeChanged(const QString& appId, int volume)
{
    m_applicationModel->updateApplicationVolume(appId, volume);
    updateGroupForApplication(appId); // Use targeted update instead of full rebuild
}

void AudioBridge::onApplicationMuteChanged(const QString& appId, bool muted)
{
    m_applicationModel->updateApplicationMute(appId, muted);
    updateGroupForApplication(appId); // Use targeted update instead of full rebuild
}

void AudioBridge::onApplicationsChanged(const QList<AudioApplication>& applications)
{
    m_applicationModel->setApplications(applications);
    updateGroupedApplications(); // Only rebuild when apps are added/removed

    QTimer::singleShot(0, this, &AudioBridge::applyChatMixIfEnabled);
}

void AudioBridge::onDevicesChanged(const QList<AudioDevice>& devices)
{
    m_outputDeviceModel->setDevices(devices);
    m_inputDeviceModel->setDevices(devices);
    updateDeviceDisplayNames();
}

void AudioBridge::onDeviceAdded(const AudioDevice& device)
{
    emit deviceAdded(device.id, device.name);
}

void AudioBridge::onDeviceRemoved(const QString& deviceId)
{
    emit deviceRemoved(deviceId);
}

void AudioBridge::onDefaultDeviceChanged(const QString& deviceId, bool isInput)
{
    emit defaultDeviceChanged(deviceId, isInput);
    updateDeviceDisplayNames();
}

void AudioBridge::onInitializationComplete()
{
    auto* manager = AudioManager::instance();
    m_outputVolume = manager->getOutputVolume();
    m_inputVolume = manager->getInputVolume();
    m_outputMuted = manager->getOutputMute();
    m_inputMuted = manager->getInputMute();

    QList<AudioApplication> apps = manager->getApplications();
    m_applicationModel->setApplications(apps);
    updateGroupedApplications(); // Initial grouping

    QList<AudioDevice> devices = manager->getDevices();
    m_outputDeviceModel->setDevices(devices);
    m_inputDeviceModel->setDevices(devices);

    m_isReady = true;

    emit outputVolumeChanged();
    emit inputVolumeChanged();
    emit outputMutedChanged();
    emit inputMutedChanged();
    emit isReadyChanged();

    applyChatMixIfEnabled();
    updateDeviceDisplayNames();
}

void AudioBridge::restoreOriginalVolumesSync()
{
    int restoreVolume = UserSettings::instance()->chatmixRestoreVolume();

    if (!m_isReady) {
        return;
    }

    auto* worker = AudioManager::instance()->getWorker();
    if (!worker) return;

    for (int i = 0; i < m_applicationModel->rowCount(); ++i) {
        QModelIndex index = m_applicationModel->index(i, 0);
        QString appId = m_applicationModel->data(index, ApplicationModel::IdRole).toString();
        QString appName = m_applicationModel->data(index, ApplicationModel::NameRole).toString();
        int streamIndex = m_applicationModel->data(index, ApplicationModel::StreamIndexRole).toInt();

        if (appName == "System sounds" || appId == "system_sounds") {
            continue;
        }

        // Skip locked applications
        if (isApplicationLocked(appName, streamIndex)) {
            continue;
        }

        QMetaObject::invokeMethod(worker, "setApplicationVolume",
                                  Qt::BlockingQueuedConnection,
                                  Q_ARG(QString, appId),
                                  Q_ARG(int, restoreVolume));
    }
}

void AudioBridge::startAudioLevelMonitoring()
{
    AudioManager::instance()->startAudioLevelMonitoring();
}

void AudioBridge::stopAudioLevelMonitoring()
{
    AudioManager::instance()->stopAudioLevelMonitoring();
}

void AudioBridge::initialize()
{
    LOG_INFO("AudioManager", "AudioBridge initialize requested");
    auto* manager = AudioManager::instance();
    manager->initialize();
}

void AudioBridge::cleanup()
{
    LOG_INFO("AudioManager", "AudioBridge cleanup requested");
    auto* manager = AudioManager::instance();
    manager->cleanup();

    // Reset cached values
    m_outputVolume = 0;
    m_inputVolume = 0;
    m_outputMuted = false;
    m_inputMuted = false;
    m_isReady = false;
    m_outputAudioLevel = 0;
    m_inputAudioLevel = 0;

    // Clear models
    m_applicationModel->setApplications(QList<AudioApplication>());
    m_groupedApplicationModel->setGroups(QList<ApplicationGroup>());
    m_outputDeviceModel->setDevices(QList<AudioDevice>());
    m_inputDeviceModel->setDevices(QList<AudioDevice>());

    // Emit signals to update QML
    emit outputVolumeChanged();
    emit inputVolumeChanged();
    emit outputMutedChanged();
    emit inputMutedChanged();
    emit isReadyChanged();
    emit outputAudioLevelChanged();
    emit inputAudioLevelChanged();
}

void AudioBridge::onOutputAudioLevelChanged(int level)
{
    if (m_outputAudioLevel != level) {
        m_outputAudioLevel = level;
        emit outputAudioLevelChanged();
    }
}

void AudioBridge::onInputAudioLevelChanged(int level)
{
    if (m_inputAudioLevel != level) {
        m_inputAudioLevel = level;
        emit inputAudioLevelChanged();
    }
}

void AudioBridge::updateGroupForApplication(const QString& appId)
{
    // Find the executable name for this appId
    QString executableName;
    for (int i = 0; i < m_applicationModel->rowCount(); ++i) {
        QModelIndex index = m_applicationModel->index(i, 0);
        if (m_applicationModel->data(index, ApplicationModel::IdRole).toString() == appId) {
            executableName = m_applicationModel->data(index, ApplicationModel::ExecutableNameRole).toString();
            break;
        }
    }

    if (executableName.isEmpty()) return;

    // Update the session model for this executable
    if (m_sessionModels.contains(executableName)) {
        // Find the session and update it
        for (int i = 0; i < m_applicationModel->rowCount(); ++i) {
            QModelIndex index = m_applicationModel->index(i, 0);
            if (m_applicationModel->data(index, ApplicationModel::IdRole).toString() == appId) {
                int volume = m_applicationModel->data(index, ApplicationModel::VolumeRole).toInt();
                bool muted = m_applicationModel->data(index, ApplicationModel::IsMutedRole).toBool();

                m_sessionModels[executableName]->updateSessionVolume(appId, volume);
                m_sessionModels[executableName]->updateSessionMute(appId, muted);
                break;
            }
        }
    }

    // Calculate new group statistics
    int totalVolume = 0;
    int mutedCount = 0;
    int sessionCount = 0;

    for (int i = 0; i < m_applicationModel->rowCount(); ++i) {
        QModelIndex index = m_applicationModel->index(i, 0);
        QString appExecutableName = m_applicationModel->data(index, ApplicationModel::ExecutableNameRole).toString();

        if (appExecutableName == executableName) {
            totalVolume += m_applicationModel->data(index, ApplicationModel::VolumeRole).toInt();
            if (m_applicationModel->data(index, ApplicationModel::IsMutedRole).toBool()) {
                mutedCount++;
            }
            sessionCount++;
        }
    }

    if (sessionCount > 0) {
        int averageVolume = totalVolume / sessionCount;
        bool anyMuted = mutedCount > 0;
        bool allMuted = mutedCount == sessionCount;

        // Update the grouped model in place
        m_groupedApplicationModel->updateGroupVolume(executableName, averageVolume);
        m_groupedApplicationModel->updateGroupMute(executableName, anyMuted, allMuted);
    }
}

QString AudioBridge::getDisplayNameForApplication(const QString& appName, int streamIndex) const
{
    for (const AppRename& rename : m_appRenames) {
        if (rename.originalName.compare(appName, Qt::CaseInsensitive) == 0 &&
            rename.streamIndex == streamIndex) {
            return rename.customName;
        }
    }

    return appName;
}

void AudioBridge::setCustomApplicationName(const QString& originalName, int streamIndex, const QString& customName)
{
    bool changed = false;

    // Check if we already have a rename for this app and index
    for (int i = 0; i < m_appRenames.count(); ++i) {
        if (m_appRenames[i].originalName.compare(originalName, Qt::CaseInsensitive) == 0 &&
            m_appRenames[i].streamIndex == streamIndex) {

            if (customName.isEmpty() || customName == originalName) {
                // Remove the rename if custom name is empty or same as original
                m_appRenames.removeAt(i);
                changed = true;
            } else if (m_appRenames[i].customName != customName) {
                // Update existing rename only if it's different
                m_appRenames[i].customName = customName;
                changed = true;
            }
            break;
        }
    }

    // Add new rename if custom name is not empty and different from original
    if (!changed && !customName.isEmpty() && customName != originalName) {
        AppRename newRename;
        newRename.originalName = originalName;
        newRename.customName = customName;
        newRename.streamIndex = streamIndex;
        m_appRenames.append(newRename);
        changed = true;
    }

    if (changed) {
        saveAppRenamesToFile();

        // Update the individual session models
        refreshApplicationDisplayNames(originalName, streamIndex);
    }
}

void AudioBridge::refreshApplicationDisplayNames(const QString& originalName, int streamIndex)
{
    // Find and update the session model that contains this application
    for (auto it = m_sessionModels.begin(); it != m_sessionModels.end(); ++it) {
        ExecutableSessionModel* sessionModel = it.value();

        // Check if this session model contains the application we just renamed
        for (int i = 0; i < sessionModel->rowCount(); ++i) {
            QModelIndex index = sessionModel->index(i, 0);
            QString appName = sessionModel->data(index, ExecutableSessionModel::NameRole).toString();
            int appStreamIndex = sessionModel->data(index, ExecutableSessionModel::StreamIndexRole).toInt();

            if (appName.compare(originalName, Qt::CaseInsensitive) == 0 && appStreamIndex == streamIndex) {
                // Force the model to update this item
                emit sessionModel->dataChanged(index, index, {ExecutableSessionModel::NameRole});
            }
        }
    }
}

QString AudioBridge::getCustomApplicationName(const QString& originalName, int streamIndex) const
{
    for (const AppRename& rename : m_appRenames) {
        if (rename.originalName.compare(originalName, Qt::CaseInsensitive) == 0 &&
            rename.streamIndex == streamIndex) {
            return rename.customName;
        }
    }
    return originalName;
}

QString AudioBridge::getAppRenamesFilePath() const
{
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appDataPath);
    return appDataPath + "/apprenames.json";
}

void AudioBridge::createDefaultAppRenames()
{
    m_appRenames.clear();

    // Add default Discord renames
    AppRename discordVoip;
    discordVoip.originalName = "Discord";
    discordVoip.customName = "Discord (VoIP)";
    discordVoip.streamIndex = 0;
    m_appRenames.append(discordVoip);

    AppRename discordUi;
    discordUi.originalName = "Discord";
    discordUi.customName = "Discord (UI)";
    discordUi.streamIndex = 1;
    m_appRenames.append(discordUi);

    saveAppRenamesToFile();
}

void AudioBridge::loadAppRenamesFromFile()
{
    QString filePath = getAppRenamesFilePath();
    QFile file(filePath);

    if (!file.exists()) {
        createDefaultAppRenames();
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        createDefaultAppRenames();
        return;
    }

    QByteArray data = file.readAll();
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError) {
        createDefaultAppRenames();
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray renamesArray = root["appRenames"].toArray();

    m_appRenames.clear();
    for (const QJsonValue& value : renamesArray) {
        QJsonObject renameObj = value.toObject();
        AppRename rename;
        rename.originalName = renameObj["originalName"].toString();
        rename.customName = renameObj["customName"].toString();
        rename.streamIndex = renameObj["streamIndex"].toInt();
        m_appRenames.append(rename);
    }

    // If file was empty or had no renames, create defaults
    if (m_appRenames.isEmpty()) {
        createDefaultAppRenames();
    }
}

void AudioBridge::saveAppRenamesToFile()
{
    QString filePath = getAppRenamesFilePath();
    QFile file(filePath);

    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }

    QJsonArray renamesArray;
    for (const AppRename& rename : m_appRenames) {
        QJsonObject renameObj;
        renameObj["originalName"] = rename.originalName;
        renameObj["customName"] = rename.customName;
        renameObj["streamIndex"] = rename.streamIndex;
        renamesArray.append(renameObj);
    }

    QJsonObject root;
    root["appRenames"] = renamesArray;

    QJsonDocument doc(root);
    file.write(doc.toJson());
}

QString AudioBridge::getCustomExecutableName(const QString& executableName) const
{
    for (const ExecutableRename& rename : m_executableRenames) {
        if (rename.originalName.compare(executableName, Qt::CaseInsensitive) == 0) {
            return rename.customName;
        }
    }
    return executableName;
}

void AudioBridge::setCustomExecutableName(const QString& executableName, const QString& customName)
{
    bool changed = false;

    // Check if we already have a rename for this executable
    for (int i = 0; i < m_executableRenames.count(); ++i) {
        if (m_executableRenames[i].originalName.compare(executableName, Qt::CaseInsensitive) == 0) {
            if (customName.isEmpty() || customName == executableName) {
                // Remove the rename if custom name is empty or same as original
                m_executableRenames.removeAt(i);
                changed = true;
            } else if (m_executableRenames[i].customName != customName) {
                // Update existing rename only if it's different
                m_executableRenames[i].customName = customName;
                changed = true;
            }
            break;
        }
    }

    // Add new rename if custom name is not empty and different from original
    if (!changed && !customName.isEmpty() && customName != executableName) {
        ExecutableRename newRename;
        newRename.originalName = executableName;
        newRename.customName = customName;
        m_executableRenames.append(newRename);
        changed = true;
    }

    if (changed) {
        saveExecutableRenamesToFile();

        // Update the grouped applications to reflect the new name
        refreshExecutableDisplayName(executableName);
    }
}

void AudioBridge::refreshExecutableDisplayName(const QString& executableName)
{
    // Update the grouped applications model
    updateGroupedApplications();
}

QString AudioBridge::getExecutableRenamesFilePath() const
{
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appDataPath);
    return appDataPath + "/executablenames.json";
}

void AudioBridge::loadExecutableRenamesFromFile()
{
    QString filePath = getExecutableRenamesFilePath();
    QFile file(filePath);

    if (!file.exists()) {
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    QByteArray data = file.readAll();
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError) {
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray renamesArray = root["executableRenames"].toArray();

    m_executableRenames.clear();
    for (const QJsonValue& value : renamesArray) {
        QJsonObject renameObj = value.toObject();
        ExecutableRename rename;
        rename.originalName = renameObj["originalName"].toString();
        rename.customName = renameObj["customName"].toString();
        m_executableRenames.append(rename);
    }
}

void AudioBridge::saveExecutableRenamesToFile()
{
    QString filePath = getExecutableRenamesFilePath();
    QFile file(filePath);

    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }

    QJsonArray renamesArray;
    for (const ExecutableRename& rename : m_executableRenames) {
        QJsonObject renameObj;
        renameObj["originalName"] = rename.originalName;
        renameObj["customName"] = rename.customName;
        renamesArray.append(renameObj);
    }

    QJsonObject root;
    root["executableRenames"] = renamesArray;

    QJsonDocument doc(root);
    file.write(doc.toJson());
}

void AudioBridge::onApplicationAudioLevelChanged(const QString& appId, int level)
{
    m_applicationAudioLevels[appId] = level;

    // Find which executable this app belongs to
    QString executableName;
    for (int i = 0; i < m_applicationModel->rowCount(); ++i) {
        QModelIndex index = m_applicationModel->index(i, 0);
        if (m_applicationModel->data(index, ApplicationModel::IdRole).toString() == appId) {
            executableName = m_applicationModel->data(index, ApplicationModel::ExecutableNameRole).toString();
            break;
        }
    }

    if (!executableName.isEmpty()) {
        updateSingleGroupAudioLevel(executableName);
    }

    emit applicationAudioLevelsChanged();
}

int AudioBridge::getApplicationAudioLevel(const QString& appId) const
{
    return m_applicationAudioLevels.value(appId, 0).toInt();
}

void AudioBridge::startApplicationAudioLevelMonitoring()
{
    AudioManager::instance()->startApplicationAudioLevelMonitoring();
}

void AudioBridge::stopApplicationAudioLevelMonitoring()
{
    AudioManager::instance()->stopApplicationAudioLevelMonitoring();
}

void AudioBridge::updateSingleGroupAudioLevel(const QString& executableName)
{
    int maxLevel = 0;
    int sessionCount = 0;

    for (int i = 0; i < m_applicationModel->rowCount(); ++i) {
        QModelIndex index = m_applicationModel->index(i, 0);
        QString appExecutableName = m_applicationModel->data(index, ApplicationModel::ExecutableNameRole).toString();

        if (appExecutableName == executableName) {
            QString appId = m_applicationModel->data(index, ApplicationModel::IdRole).toString();
            int currentLevel = getApplicationAudioLevel(appId);
            maxLevel = qMax(maxLevel, currentLevel);
            sessionCount++;
        }
    }

    m_groupedApplicationModel->updateGroupAudioLevel(executableName, maxLevel);
}

bool AudioBridge::isApplicationLocked(const QString& originalName, int streamIndex) const
{
    for (const AppLock& lock : m_appLocks) {
        if (lock.originalName.compare(originalName, Qt::CaseInsensitive) == 0 &&
            lock.streamIndex == streamIndex) {
            return lock.isLocked;
        }
    }
    return false;
}

void AudioBridge::setApplicationLocked(const QString& originalName, int streamIndex, bool locked)
{
    bool changed = false;

    // Check if we already have a lock entry for this app and index
    for (int i = 0; i < m_appLocks.count(); ++i) {
        if (m_appLocks[i].originalName.compare(originalName, Qt::CaseInsensitive) == 0 &&
            m_appLocks[i].streamIndex == streamIndex) {

            if (!locked) {
                // Remove the lock if unlocking
                m_appLocks.removeAt(i);
                changed = true;
            } else if (m_appLocks[i].isLocked != locked) {
                // Update existing lock
                m_appLocks[i].isLocked = locked;
                changed = true;
            }
            break;
        }
    }

    // Add new lock if locking and not found
    if (!changed && locked) {
        AppLock newLock;
        newLock.originalName = originalName;
        newLock.streamIndex = streamIndex;
        newLock.isLocked = true;
        m_appLocks.append(newLock);
        changed = true;
    }

    if (changed) {
        saveAppLocksToFile();
        // Force refresh similar to rename functionality
        refreshApplicationDisplayNames(originalName, streamIndex);
        // Emit signal for real-time updates
        emit applicationLockChanged(originalName, streamIndex, locked);
    }
}

QString AudioBridge::getAppLocksFilePath() const
{
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appDataPath);
    return appDataPath + "/applocks.json";
}

void AudioBridge::loadAppLocksFromFile()
{
    QString filePath = getAppLocksFilePath();
    QFile file(filePath);

    if (!file.exists()) {
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    QByteArray data = file.readAll();
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError) {
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray locksArray = root["appLocks"].toArray();

    m_appLocks.clear();
    for (const QJsonValue& value : locksArray) {
        QJsonObject lockObj = value.toObject();
        AppLock lock;
        lock.originalName = lockObj["originalName"].toString();
        lock.streamIndex = lockObj["streamIndex"].toInt();
        lock.isLocked = lockObj["isLocked"].toBool();
        m_appLocks.append(lock);
    }
}

void AudioBridge::saveAppLocksToFile()
{
    QString filePath = getAppLocksFilePath();
    QFile file(filePath);

    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }

    QJsonArray locksArray;
    for (const AppLock& lock : m_appLocks) {
        QJsonObject lockObj;
        lockObj["originalName"] = lock.originalName;
        lockObj["streamIndex"] = lock.streamIndex;
        lockObj["isLocked"] = lock.isLocked;
        locksArray.append(lockObj);
    }

    QJsonObject root;
    root["appLocks"] = locksArray;

    QJsonDocument doc(root);
    file.write(doc.toJson());
}

void AudioBridge::loadDeviceRenamesFromFile()
{
    QString filePath = getDeviceRenamesFilePath();
    QFile file(filePath);

    if (!file.exists()) {
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    QByteArray data = file.readAll();
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError) {
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray renamesArray = root["deviceRenames"].toArray();

    m_deviceRenames.clear();
    for (const QJsonValue& value : renamesArray) {
        QJsonObject renameObj = value.toObject();
        DeviceRename rename;
        rename.originalName = renameObj["originalName"].toString();
        rename.customName = renameObj["customName"].toString();
        m_deviceRenames.append(rename);
    }
}

void AudioBridge::saveDeviceRenamesToFile()
{
    QString filePath = getDeviceRenamesFilePath();
    QFile file(filePath);

    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }

    QJsonArray renamesArray;
    for (const DeviceRename& rename : m_deviceRenames) {
        QJsonObject renameObj;
        renameObj["originalName"] = rename.originalName;
        renameObj["customName"] = rename.customName;
        renamesArray.append(renameObj);
    }

    QJsonObject root;
    root["deviceRenames"] = renamesArray;

    QJsonDocument doc(root);
    file.write(doc.toJson());
}

QString AudioBridge::getDeviceRenamesFilePath() const
{
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appDataPath);
    return appDataPath + "/devicerenames.json";
}

QString AudioBridge::getDisplayNameForDevice(const QString& deviceName) const
{
    for (const DeviceRename& rename : m_deviceRenames) {
        if (rename.originalName.compare(deviceName, Qt::CaseInsensitive) == 0) {
            return rename.customName;
        }
    }
    return deviceName;
}

void AudioBridge::setCustomDeviceName(const QString& originalName, const QString& customName)
{
    bool changed = false;

    for (int i = 0; i < m_deviceRenames.count(); ++i) {
        if (m_deviceRenames[i].originalName.compare(originalName, Qt::CaseInsensitive) == 0) {
            if (customName.isEmpty() || customName == originalName) {
                m_deviceRenames.removeAt(i);
                changed = true;
            } else if (m_deviceRenames[i].customName != customName) {
                m_deviceRenames[i].customName = customName;
                changed = true;
            }
            break;
        }
    }

    if (!changed && !customName.isEmpty() && customName != originalName) {
        DeviceRename newRename;
        newRename.originalName = originalName;
        newRename.customName = customName;
        m_deviceRenames.append(newRename);
        changed = true;
    }

    if (changed) {
        saveDeviceRenamesToFile();
        updateDeviceDisplayNames();
        emit deviceRenameUpdated();
        refreshDeviceModelData(originalName);
    }
}

QString AudioBridge::getCustomDeviceName(const QString& originalName) const
{
    for (const DeviceRename& rename : m_deviceRenames) {
        if (rename.originalName.compare(originalName, Qt::CaseInsensitive) == 0) {
            return rename.customName;
        }
    }
    return originalName;
}

void AudioBridge::updateDeviceDisplayNames()
{
    // Update output device display name
    QString newOutputName = "";
    if (m_isReady) {
        int defaultIndex = m_outputDeviceModel->getCurrentDefaultIndex();
        if (defaultIndex >= 0) {
            QString originalName = m_outputDeviceModel->getDeviceName(defaultIndex);
            newOutputName = getDisplayNameForDevice(originalName);
        }
    }

    if (m_outputDeviceDisplayName != newOutputName) {
        m_outputDeviceDisplayName = newOutputName;
        emit outputDeviceDisplayNameChanged();
    }

    // Update input device display name
    QString newInputName = "";
    if (m_isReady) {
        int defaultIndex = m_inputDeviceModel->getCurrentDefaultIndex();
        if (defaultIndex >= 0) {
            QString originalName = m_inputDeviceModel->getDeviceName(defaultIndex);
            newInputName = getDisplayNameForDevice(originalName);
        }
    }

    if (m_inputDeviceDisplayName != newInputName) {
        m_inputDeviceDisplayName = newInputName;
        emit inputDeviceDisplayNameChanged();
    }
}

void AudioBridge::refreshDeviceDisplayNames()
{
    updateDeviceDisplayNames();
}

void AudioBridge::refreshDeviceModelData(const QString& originalName)
{
    // Check output devices and emit dataChanged for matching devices
    for (int i = 0; i < m_outputDeviceModel->rowCount(); ++i) {
        QModelIndex index = m_outputDeviceModel->index(i, 0);
        QString deviceName = m_outputDeviceModel->data(index, FilteredDeviceModel::NameRole).toString();
        if (deviceName == originalName) {
            emit m_outputDeviceModel->dataChanged(index, index, {FilteredDeviceModel::NameRole});
        }
    }

    // Check input devices and emit dataChanged for matching devices
    for (int i = 0; i < m_inputDeviceModel->rowCount(); ++i) {
        QModelIndex index = m_inputDeviceModel->index(i, 0);
        QString deviceName = m_inputDeviceModel->data(index, FilteredDeviceModel::NameRole).toString();
        if (deviceName == originalName) {
            emit m_inputDeviceModel->dataChanged(index, index, {FilteredDeviceModel::NameRole});
        }
    }
}

void AudioBridge::loadDeviceIconsFromFile()
{
    QString filePath = getDeviceIconsFilePath();
    QFile file(filePath);

    if (!file.exists()) {
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    QByteArray data = file.readAll();
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError) {
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray iconsArray = root["deviceIcons"].toArray();

    m_deviceIcons.clear();
    for (const QJsonValue& value : iconsArray) {
        QJsonObject iconObj = value.toObject();
        DeviceIcon icon;
        icon.originalName = iconObj["originalName"].toString();
        icon.iconName = iconObj["iconName"].toString();
        m_deviceIcons.append(icon);
    }
}

void AudioBridge::saveDeviceIconsToFile()
{
    QString filePath = getDeviceIconsFilePath();
    QFile file(filePath);

    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }

    QJsonArray iconsArray;
    for (const DeviceIcon& icon : m_deviceIcons) {
        QJsonObject iconObj;
        iconObj["originalName"] = icon.originalName;
        iconObj["iconName"] = icon.iconName;
        iconsArray.append(iconObj);
    }

    QJsonObject root;
    root["deviceIcons"] = iconsArray;

    QJsonDocument doc(root);
    file.write(doc.toJson());
}

QString AudioBridge::getDeviceIconsFilePath() const
{
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appDataPath);
    return appDataPath + "/deviceicons.json";
}

void AudioBridge::setCustomDeviceIcon(const QString& originalName, const QString& iconName)
{
    bool changed = false;

    for (int i = 0; i < m_deviceIcons.count(); ++i) {
        if (m_deviceIcons[i].originalName.compare(originalName, Qt::CaseInsensitive) == 0) {
            if (iconName.isEmpty()) {
                m_deviceIcons.removeAt(i);
                changed = true;
            } else if (m_deviceIcons[i].iconName != iconName) {
                m_deviceIcons[i].iconName = iconName;
                changed = true;
            }
            break;
        }
    }

    if (!changed && !iconName.isEmpty()) {
        DeviceIcon newIcon;
        newIcon.originalName = originalName;
        newIcon.iconName = iconName;
        m_deviceIcons.append(newIcon);
        changed = true;
    }

    if (changed) {
        saveDeviceIconsToFile();
        emit deviceIconUpdated();
        refreshDeviceModelData(originalName);
    }
}

QString AudioBridge::getCustomDeviceIcon(const QString& originalName) const
{
    for (const DeviceIcon& icon : m_deviceIcons) {
        if (icon.originalName.compare(originalName, Qt::CaseInsensitive) == 0) {
            return icon.iconName;
        }
    }
    return QString();
}

QString AudioBridge::getDisplayIconForDevice(const QString& deviceName, bool isInput) const
{
    // Check for custom icon first
    QString customIcon = getCustomDeviceIcon(deviceName);
    if (!customIcon.isEmpty()) {
        return QString("qrc:/icons/devices/%1.png").arg(customIcon);
    }

    // Get device description for better matching
    QString deviceDescription;

    // Search in both input and output models
    for (FilteredDeviceModel* model : {m_inputDeviceModel, m_outputDeviceModel}) {
        for (int i = 0; i < model->rowCount(); ++i) {
            QModelIndex index = model->index(i, 0);
            QString modelDeviceName = model->data(index, FilteredDeviceModel::NameRole).toString();
            if (modelDeviceName.compare(deviceName, Qt::CaseInsensitive) == 0) {
                deviceDescription = model->data(index, FilteredDeviceModel::DescriptionRole).toString();
                break;
            }
        }
        if (!deviceDescription.isEmpty()) break;
    }

    // Use description for icon matching
    QString searchText = deviceDescription.toLower();

    // Return icon based on description keywords
    QString iconName;
    if (searchText.contains("headset") && searchText.contains("microphone")) {
        iconName = "headset-mic";
    } else if (searchText.contains("headset")) {
        iconName = "headset";
    } else if (searchText.contains("headset") && searchText.contains("earphone")) {
        iconName = "headset-mic";
    } else if (searchText.contains("microphone")) {
        iconName = "microphone";
    } else if (searchText.contains("speaker")) {
        iconName = "speaker";
    } else {
        // Fallback to original logic if no description match
        iconName = isInput ? "microphone" : "speaker";
    }

    return QString("qrc:/icons/devices/%1.png").arg(iconName);
}

bool AudioBridge::isApplicationMutedInBackground(const QString& executableName) const
{
    return m_windowFocusManager->isApplicationMutedInBackground(executableName);
}

void AudioBridge::setApplicationMutedInBackground(const QString& executableName, bool muted)
{
    m_windowFocusManager->setApplicationMutedInBackground(executableName, muted);
}

void AudioBridge::onApplicationFocusChanged(const QString& executableName, bool hasFocus)
{
    if (!m_windowFocusManager->isApplicationMutedInBackground(executableName)) {
        return;
    }

    // Find all applications with this executable name
    for (int i = 0; i < m_applicationModel->rowCount(); ++i) {
        QModelIndex index = m_applicationModel->index(i, 0);
        QString appExecutableName = m_applicationModel->data(index, ApplicationModel::ExecutableNameRole).toString();

        if (appExecutableName.compare(executableName, Qt::CaseInsensitive) == 0) {
            QString appId = m_applicationModel->data(index, ApplicationModel::IdRole).toString();
            bool currentMuteState = m_applicationModel->data(index, ApplicationModel::IsMutedRole).toBool();

            if (!hasFocus) {
                // Application lost focus - mute it and store original state
                if (!currentMuteState) {
                    m_originalMuteStates[appId] = false;
                    setApplicationMute(appId, true);
                }
            } else {
                // Application gained focus - restore original state
                if (m_originalMuteStates.contains(appId)) {
                    bool originalState = m_originalMuteStates[appId];
                    setApplicationMute(appId, originalState);
                    m_originalMuteStates.remove(appId);
                }
            }
        }
    }
}

AudioDevice AudioBridge::getCurrentOutputDevice() const
{
    if (!m_isReady || !m_outputDeviceModel) {
        return AudioDevice();
    }

    int defaultIndex = m_outputDeviceModel->getCurrentDefaultIndex();
    if (defaultIndex < 0) {
        return AudioDevice();
    }

    QModelIndex modelIndex = m_outputDeviceModel->index(defaultIndex, 0);
    AudioDevice device;
    device.id = m_outputDeviceModel->data(modelIndex, FilteredDeviceModel::IdRole).toString();
    device.name = m_outputDeviceModel->data(modelIndex, FilteredDeviceModel::NameRole).toString();
    device.vendorId = m_outputDeviceModel->data(modelIndex, FilteredDeviceModel::VendorIdRole).toString();
    device.productId = m_outputDeviceModel->data(modelIndex, FilteredDeviceModel::ProductIdRole).toString();
    device.isDefault = true;
    device.isInput = false;

    return device;
}

AudioDevice AudioBridge::getCurrentInputDevice() const
{
    if (!m_isReady || !m_inputDeviceModel) {
        return AudioDevice();
    }

    int defaultIndex = m_inputDeviceModel->getCurrentDefaultIndex();
    if (defaultIndex < 0) {
        return AudioDevice();
    }

    QModelIndex modelIndex = m_inputDeviceModel->index(defaultIndex, 0);
    AudioDevice device;
    device.id = m_inputDeviceModel->data(modelIndex, FilteredDeviceModel::IdRole).toString();
    device.name = m_inputDeviceModel->data(modelIndex, FilteredDeviceModel::NameRole).toString();
    device.vendorId = m_inputDeviceModel->data(modelIndex, FilteredDeviceModel::VendorIdRole).toString();
    device.productId = m_inputDeviceModel->data(modelIndex, FilteredDeviceModel::ProductIdRole).toString();
    device.isDefault = true;
    device.isInput = true;

    return device;
}
