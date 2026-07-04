#pragma once

#include <QGraphicsObject>
#include <QColor>
#include "core/GameTypes.h"

class TileItem : public QGraphicsObject {
    Q_OBJECT

public:
    explicit TileItem(int tileId, QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter,
               const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;

    int tileId() const { return m_tileId; }

    void setSelectedVisual(bool selected);
    void setHoveredVisual(bool hovered);
    void setOwnerColor(const QColor& color);
    void setOwnerInfo(int ownerId, const QColor& color);
    void setShopType(ShopType type);
    void setShopInfo(ShopType type, int ownerId, const QColor& color);
    void setBuildable(bool buildable);
    void setTradeTarget(bool enabled);
    void setChoiceCandidate(bool enabled);
    void setChoiceSelected(bool enabled);
    void setChoiceBlinkOn(bool enabled);

signals:
    void clicked(int tileId);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private:
    int m_tileId = -1;
    bool m_selected = false;
    bool m_hovered = false;
    bool m_buildable = false;
    bool m_tradeTarget = false;
    bool m_choiceCandidate = false;
    bool m_choiceSelected = false;
    bool m_choiceBlinkOn = true;
    int m_ownerId = -1;
    int m_shopOwnerId = -1;
    QColor m_ownerColor;
    QColor m_shopOwnerColor;
    ShopType m_shopType = ShopType::None;
};
