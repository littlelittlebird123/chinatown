#pragma once

#include <QTextEdit>

class GameLogPanel : public QTextEdit {
    Q_OBJECT

public:
    explicit GameLogPanel(QWidget* parent = nullptr);

public slots:
    void appendMessage(const QString& message);
    void clearLog();
};
