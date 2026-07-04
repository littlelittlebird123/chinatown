#pragma once

#include <QGraphicsView>
#include <QHash>
#include <QSet>

class GameController;
class QGraphicsScene;
class QGraphicsPixmapItem;
class QResizeEvent;
class QTimer;
class TileItem;

class BoardView : public QGraphicsView {
    Q_OBJECT

public:
    explicit BoardView(QWidget* parent = nullptr);

    void setController(GameController* controller);

signals:
    void tileClicked(int tileId);

public slots:
    void rebuildBoard();
    void refreshAll();
    void refreshTile(int tileId);
    void highlightTile(int tileId);

    void clearTileEffects();
    void showBuildableTiles(const QList<int>& tileIds);
    void showChoiceCandidateTiles(const QList<int>& tileIds);
    void setChoiceSelectedTiles(const QList<int>& tileIds);
    void showTradeTargetTiles(const QList<int>& tileIds);
    void setTradeSelectedTiles(const QList<int>& tileIds);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void fitBoardToViewport();
    void applyChoiceBlink();

    GameController* m_controller = nullptr;
    QGraphicsScene* m_scene = nullptr;
    QGraphicsPixmapItem* m_backgroundItem = nullptr;
    QHash<int, TileItem*> m_items;
    QSet<int> m_choiceCandidateIds;
    QTimer* m_choiceBlinkTimer = nullptr;
    bool m_choiceBlinkOn = true;
};
