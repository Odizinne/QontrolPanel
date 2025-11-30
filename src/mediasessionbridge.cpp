#include "mediasessionbridge.h"
#include "mediasessionmanager.h"

MediaSessionBridge* MediaSessionBridge::m_instance = nullptr;

MediaSessionBridge::MediaSessionBridge(QObject* parent)
    : QObject(parent)
{
    if (MediaSessionManager::getWorker()) {
        connect(MediaSessionManager::getWorker(), &MediaWorker::mediaInfoChanged,
                this, [this](const MediaInfo& info) {
                    m_mediaTitle = info.title;
                    m_mediaArtist = info.artist;
                    m_mediaArt = info.albumArt;
                    m_isMediaPlaying = info.isPlaying;
                    emit mediaInfoChanged();
                });
    }
}

MediaSessionBridge::~MediaSessionBridge()
{
    if (m_instance == this) {
        m_instance = nullptr;
    }
}

MediaSessionBridge* MediaSessionBridge::create(QQmlEngine* qmlEngine, QJSEngine* jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)

    if (!m_instance) {
        m_instance = new MediaSessionBridge();
    }
    return m_instance;
}

MediaSessionBridge* MediaSessionBridge::instance()
{
    return m_instance;
}

QString MediaSessionBridge::mediaTitle() const {
    return m_mediaTitle;
}

QString MediaSessionBridge::mediaArtist() const {
    return m_mediaArtist;
}

bool MediaSessionBridge::isMediaPlaying() const {
    return m_isMediaPlaying;
}

QString MediaSessionBridge::mediaArt() const {
    return m_mediaArt;
}

void MediaSessionBridge::playPause() {
    MediaSessionManager::playPauseAsync();
}

void MediaSessionBridge::nextTrack() {
    MediaSessionManager::nextTrackAsync();
}

void MediaSessionBridge::previousTrack() {
    MediaSessionManager::previousTrackAsync();
}

void MediaSessionBridge::startMediaMonitoring() {
    MediaSessionManager::startMonitoringAsync();
}

void MediaSessionBridge::stopMediaMonitoring() {
    MediaSessionManager::stopMonitoringAsync();
}
