#include "utils/AssetManager.h"

#include <QDir>
#include <QFile>
#include <QPainter>

AssetManager& AssetManager::instance()
{
    static AssetManager manager;
    return manager;
}

QPixmap AssetManager::pixmap(const QString& key)
{
    if (m_cache.contains(key)) {
        return m_cache.value(key);
    }

    QPixmap pix;
    const QString resourcePath = QStringLiteral(":/images/%1").arg(key);
    const QString filePath = QStringLiteral("resources/images/%1").arg(key);

    if (QFile::exists(resourcePath)) {
        pix.load(resourcePath);
    } else if (QFile::exists(filePath)) {
        pix.load(filePath);
    }

    if (pix.isNull()) {
        pix = makePlaceholder(key);
    }

    m_cache.insert(key, pix);
    return pix;
}

QPixmap AssetManager::scaledPixmap(const QString& key, int scale)
{
    QPixmap source = pixmap(key);
    if (scale <= 1) {
        return source;
    }

    return source.scaled(source.width() * scale,
                         source.height() * scale,
                         Qt::IgnoreAspectRatio,
                         Qt::FastTransformation);
}

bool AssetManager::hasPixmap(const QString& key) const
{
    const QString resourcePath = QStringLiteral(":/images/%1").arg(key);
    const QString filePath = QStringLiteral("resources/images/%1").arg(key);
    return QFile::exists(resourcePath) || QFile::exists(filePath);
}

QPixmap AssetManager::makePlaceholder(const QString& key) const
{
    QPixmap pix(32, 32);
    pix.fill(QColor(230, 220, 190));

    QPainter painter(&pix);
    painter.setPen(QPen(QColor(80, 70, 60), 2));
    painter.drawRect(1, 1, 30, 30);
    painter.setPen(QColor(80, 70, 60));
    painter.drawText(pix.rect(), Qt::AlignCenter, key.left(2).toUpper());

    return pix;
}
