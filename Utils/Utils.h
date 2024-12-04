#ifndef UTILS_H
#define UTILS_H

#include <QIcon>
#include <QFrame>

namespace Utils {
    QIcon getIcon(int type, int volume, bool muted);
    void setFrameColorBasedOnWindow(QWidget *window, QFrame *frame);
    QColor adjustColor(const QColor &color, double factor);
    bool isDarkMode(const QColor &color);
    QIcon generateMutedIcon(QPixmap originalPixmap);
    QString getAccentColor(const QString &accentKey);
    QString getTheme();
    QIcon getPlaceholderIcon();
    QIcon getButtonsIcon(QString button);
    void setAlwaysOnTopState(QWidget *widget, bool state);
    QString truncateTitle(QString title, QFontMetrics metrics, int labelWidth);
    QString getOverlayIcon(int volume);
}

#endif // UTILS_H
