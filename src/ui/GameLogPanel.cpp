#include "ui/GameLogPanel.h"

#include <QDateTime>

GameLogPanel::GameLogPanel(QWidget* parent)
    : QTextEdit(parent)
{
    setReadOnly(true);
    setMinimumHeight(140);
    setPlaceholderText(QStringLiteral("游戏日志"));
}

void GameLogPanel::appendMessage(const QString& message)
{
    const QString time = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    append(QStringLiteral("[%1] %2").arg(time, message));
}

void GameLogPanel::clearLog()
{
    clear();
}
