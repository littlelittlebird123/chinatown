#include "ui/BoardTheme.h"

QList<BoardTheme> availableBoardThemes()
{
    return {
        {
            QStringLiteral("porcelain"),
            QStringLiteral("蓝白瓷城"),
            QStringLiteral(":/images/board/chinatown_board_porcelain.jpg"),
            QPointF(-5.0, 5.0)
        },
        {
            QStringLiteral("lantern_city"),
            QStringLiteral("灯火唐城"),
            QStringLiteral(":/images/board/chinatown_board_lantern_city.png"),
            QPointF(-5.0, -5.0)
        },
        {
            QStringLiteral("emerald"),
            QStringLiteral("翡翠之城"),
            QStringLiteral(":/images/board/chinatown_board_emerald.jpg"),
            QPointF(-5.0, 5.0)
        },
        {
            QStringLiteral("black_gold"),
            QStringLiteral("玄金华坊"),
            QStringLiteral(":/images/board/chinatown_board_black_gold.jpg"),
            QPointF(-5.0, 5.0)
        },
        {
            QStringLiteral("qingxiao_dragon"),
            QStringLiteral("青霄龙坊"),
            QStringLiteral(":/images/board/chinatown_board_qingxiao_dragon.jpg"),
            QPointF(-5.0, 5.0)
        }
    };
}

QString defaultBoardThemeKey()
{
    return QStringLiteral("porcelain");
}

BoardTheme boardThemeByKey(const QString& key)
{
    const QList<BoardTheme> themes = availableBoardThemes();
    for (const BoardTheme& theme : themes) {
        if (theme.key == key) {
            return theme;
        }
    }
    return themes.first();
}
