#pragma once

#include <QHash>
#include <QPixmap>
#include <QString>

class AssetManager {
public:
    static AssetManager& instance();

    QPixmap pixmap(const QString& key);
    QPixmap scaledPixmap(const QString& key, int scale);
    bool hasPixmap(const QString& key) const;

private:
    AssetManager() = default;

    QPixmap makePlaceholder(const QString& key) const;

    mutable QHash<QString, QPixmap> m_cache;
};
