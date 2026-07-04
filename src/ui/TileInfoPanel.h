#pragma once

#include <QLabel>
#include <QPushButton>
#include <QWidget>

class GameController;

class TileInfoPanel : public QWidget {
    Q_OBJECT

public:
    explicit TileInfoPanel(QWidget* parent = nullptr);

    void setController(GameController* controller);
    void showTile(int tileId);
    void clear();

signals:
    void tradeWithOwnerRequested(int ownerId);
    void buildHereRequested(int tileId);

private:
    GameController* m_controller = nullptr;
    int m_tileId = -1;

    QLabel* m_titleLabel = nullptr;
    QLabel* m_infoLabel = nullptr;
};
