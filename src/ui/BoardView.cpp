#include "ui/BoardView.h"

#include "core/GameController.h"
#include "ui/BoardTheme.h"
#include "core/GameTypes.h"
#include "ui/TileItem.h"

#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTimer>

BoardView::BoardView(QWidget* parent)
    : QGraphicsView(parent)
    , m_scene(new QGraphicsScene(this))
{
    setScene(m_scene);
    setRenderHint(QPainter::Antialiasing, false);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    setBackgroundBrush(QColor(76, 65, 52));
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_choiceBlinkTimer = new QTimer(this);
    m_choiceBlinkTimer->setInterval(360);
    connect(m_choiceBlinkTimer, &QTimer::timeout, this, &BoardView::applyChoiceBlink);
    m_choiceBlinkTimer->start();
}

void BoardView::setController(GameController* controller)
{
    m_controller = controller;
    rebuildBoard();
}

void BoardView::rebuildBoard()
{
    m_scene->clear();
    m_items.clear();
    m_backgroundItem = nullptr;

    if (!m_controller) {
        return;
    }

    const BoardTheme theme = boardThemeByKey(m_controller->boardThemeKey());
    const QPixmap bgPixmap(theme.resourcePath);
    if (!bgPixmap.isNull()) {
        m_backgroundItem = m_scene->addPixmap(bgPixmap);
        m_backgroundItem->setZValue(-100.0);
        m_scene->setSceneRect(bgPixmap.rect());
    }

    const QList<Tile> tiles = m_controller->tiles();
    for (const Tile& tile : tiles) {
        auto* item = new TileItem(tile.id);
        item->setPos(QPointF(tile.gridPos) + theme.tileOffset);
        item->setZValue(10.0);
        connect(item, &TileItem::clicked, this, &BoardView::tileClicked);
        m_scene->addItem(item);
        m_items.insert(tile.id, item);
        refreshTile(tile.id);
    }

    if (m_scene->sceneRect().isEmpty()) {
        m_scene->setSceneRect(m_scene->itemsBoundingRect().adjusted(-30, -30, 30, 30));
    }

    fitBoardToViewport();
}

void BoardView::refreshAll()
{
    if (!m_controller) {
        return;
    }

    for (const Tile& tile : m_controller->tiles()) {
        refreshTile(tile.id);
    }
    highlightTile(m_controller->selectedTileId());
}

void BoardView::refreshTile(int tileId)
{
    if (!m_controller) {
        return;
    }

    TileItem* item = m_items.value(tileId, nullptr);
    const Tile* tile = m_controller->tileById(tileId);
    if (!item || !tile) {
        return;
    }

    if (const Player* owner = m_controller->playerById(tile->ownerId)) {
        item->setOwnerInfo(owner->id, owner->color);
    } else {
        item->setOwnerInfo(-1, QColor());
    }

    if (tile->shopType != ShopType::None) {
        if (const Player* shopOwner = m_controller->playerById(tile->shopOwnerId)) {
            item->setShopInfo(tile->shopType, shopOwner->id, shopOwner->color);
        } else {
            item->setShopInfo(tile->shopType, -1, QColor());
        }
    } else {
        item->setShopInfo(ShopType::None, -1, QColor());
    }

    item->setSelectedVisual(m_controller->selectedTileId() == tileId);
}

void BoardView::highlightTile(int tileId)
{
    for (auto it = m_items.begin(); it != m_items.end(); ++it) {
        it.value()->setSelectedVisual(it.key() == tileId);
    }
}

void BoardView::clearTileEffects()
{
    m_choiceCandidateIds.clear();
    for (auto it = m_items.begin(); it != m_items.end(); ++it) {
        TileItem* item = it.value();
        item->setBuildable(false);
        item->setTradeTarget(false);
        item->setChoiceCandidate(false);
        item->setChoiceSelected(false);
        item->setChoiceBlinkOn(true);
    }
}

void BoardView::showBuildableTiles(const QList<int>& tileIds)
{
    for (int tileId : tileIds) {
        if (TileItem* item = m_items.value(tileId, nullptr)) {
            item->setBuildable(true);
        }
    }
}

void BoardView::showChoiceCandidateTiles(const QList<int>& tileIds)
{
    m_choiceCandidateIds.clear();
    for (int tileId : tileIds) {
        m_choiceCandidateIds.insert(tileId);
        if (TileItem* item = m_items.value(tileId, nullptr)) {
            item->setChoiceCandidate(true);
            item->setChoiceBlinkOn(m_choiceBlinkOn);
        }
    }
}

void BoardView::setChoiceSelectedTiles(const QList<int>& tileIds)
{
    const QSet<int> selectedSet(tileIds.begin(), tileIds.end());
    for (auto it = m_items.begin(); it != m_items.end(); ++it) {
        it.value()->setChoiceSelected(selectedSet.contains(it.key()));
    }
}

void BoardView::showTradeTargetTiles(const QList<int>& tileIds)
{
    for (int tileId : tileIds) {
        if (TileItem* item = m_items.value(tileId, nullptr)) {
            item->setTradeTarget(true);
        }
    }
}

void BoardView::setTradeSelectedTiles(const QList<int>& tileIds)
{
    const QSet<int> selectedSet(tileIds.begin(), tileIds.end());
    for (auto it = m_items.begin(); it != m_items.end(); ++it) {
        it.value()->setChoiceSelected(selectedSet.contains(it.key()));
    }
}

void BoardView::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);
    fitBoardToViewport();
}

void BoardView::fitBoardToViewport()
{
    if (!m_scene || m_scene->sceneRect().isEmpty()) {
        return;
    }

    resetTransform();
    fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
}

void BoardView::applyChoiceBlink()
{
    m_choiceBlinkOn = !m_choiceBlinkOn;

    
    for (auto it = m_items.constBegin(); it != m_items.constEnd(); ++it) {
        if (it.value()) {
            it.value()->setChoiceBlinkOn(m_choiceBlinkOn);
        }
    }
}
