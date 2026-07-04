#pragma once

#include <QList>
#include <QPointF>
#include <QString>

struct BoardTheme {
    QString key;
    QString displayName;
    QString resourcePath;
    QPointF tileOffset;
};

QList<BoardTheme> availableBoardThemes();
BoardTheme boardThemeByKey(const QString& key);
QString defaultBoardThemeKey();
