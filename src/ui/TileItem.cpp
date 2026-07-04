#include "ui/TileItem.h"

#include "utils/AssetManager.h"

#include <QGraphicsSceneMouseEvent>
#include <QPainter>

TileItem::TileItem(int tileId, QGraphicsItem* parent)
    : QGraphicsObject(parent)
    , m_tileId(tileId)
{
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
}

QRectF TileItem::boundingRect() const
{
    return QRectF(0, 0, 86, 92);
}

void TileItem::paint(QPainter* painter,
                     const QStyleOptionGraphicsItem* option,
                     QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    painter->setRenderHint(QPainter::Antialiasing, false);

    QRectF r = boundingRect().adjusted(1, 1, -1, -1);

    
    if (m_shopType == ShopType::None) {
        QColor overlay;
        if (m_choiceSelected) {
            overlay = QColor(255, 188, 70, 72);
        } else if (m_choiceCandidate) {
            overlay = m_choiceBlinkOn ? QColor(255, 218, 90, 58) : QColor(255, 218, 90, 18);
        } else if (m_tradeTarget) {
            overlay = m_choiceBlinkOn ? QColor(75, 130, 230, 72) : QColor(75, 130, 230, 18);
        } else if (m_selected || m_hovered) {
            overlay = QColor(255, 248, 218, 36);
        }
        if (overlay.isValid()) {
            painter->fillRect(r, overlay);
        }
    }

    if (m_shopType != ShopType::None) {
        QRectF shopRect = r.adjusted(2, 2, -2, -2);
        const QString shopAssetKey = QStringLiteral("shops/%1.png").arg(shopTypeKey(m_shopType));
        if (AssetManager::instance().hasPixmap(shopAssetKey)) {
            const QPixmap src = AssetManager::instance().pixmap(shopAssetKey);
            const QPixmap pix = src.scaled(shopRect.size().toSize(),
                                           Qt::IgnoreAspectRatio,
                                           Qt::SmoothTransformation);
            painter->drawPixmap(shopRect.topLeft(), pix);
        } else {
            painter->fillRect(shopRect, QColor(252, 244, 218));
            painter->setPen(QColor(40, 35, 35));
            painter->drawText(shopRect, Qt::AlignCenter, shopTypeName(m_shopType).left(2));
        }
    }

    
    
    
    const QColor ownerBorderColor = m_ownerColor.isValid() ? m_ownerColor : QColor(95, 78, 55, 190);
    const int brightness = ownerBorderColor.red() + ownerBorderColor.green() + ownerBorderColor.blue();
    const bool isDarkOwnerBorder = m_ownerColor.isValid() && (m_ownerId == 3 || brightness < 96);
    painter->setBrush(Qt::NoBrush);
    if (m_ownerColor.isValid()) {
        const QColor outerStroke = isDarkOwnerBorder ? QColor(232, 205, 142, 230) : QColor(30, 22, 16, 180);
        painter->setPen(QPen(outerStroke, isDarkOwnerBorder ? 12 : 10));
        painter->drawRect(r.adjusted(-1, -1, 1, 1));
        painter->setPen(QPen(ownerBorderColor, isDarkOwnerBorder ? 8 : 7));
    } else {
        painter->setPen(QPen(ownerBorderColor, 3));
    }
    painter->drawRect(r);

    
    QPen accentPen(Qt::NoPen);
    if (m_selected) {
        accentPen = QPen(QColor(255, 190, 40), 4);
    } else if (m_choiceSelected) {
        accentPen = QPen(QColor(255, 128, 0), 4);
    } else if (m_choiceCandidate) {
        accentPen = QPen(m_choiceBlinkOn ? QColor(235, 160, 35) : QColor(255, 210, 85), m_choiceBlinkOn ? 4 : 2);
    } else if (m_tradeTarget) {
        accentPen = QPen(m_choiceBlinkOn ? QColor(70, 120, 220) : QColor(145, 185, 255), m_choiceBlinkOn ? 4 : 2);
    }

    if (accentPen.style() != Qt::NoPen) {
        painter->setPen(accentPen);
        painter->drawRect(r.adjusted(5, 5, -5, -5));
    }

    if (m_buildable) {
        
        const QColor glowColor = m_choiceBlinkOn ? QColor(255, 215, 95, 150) : QColor(255, 215, 95, 60);
        painter->setPen(QPen(glowColor, m_choiceBlinkOn ? 12 : 7));
        painter->drawRect(r.adjusted(7, 7, -7, -7));

        QPen dashPen(QColor(255, 224, 92, m_choiceBlinkOn ? 235 : 130), m_choiceBlinkOn ? 4 : 3);
        dashPen.setStyle(Qt::DashLine);
        painter->setPen(dashPen);
        painter->drawRect(r.adjusted(9, 9, -9, -9));
    }

    if (m_choiceSelected) {
        painter->setPen(QPen(QColor(90, 55, 15), 2));
        painter->drawText(QRectF(r.right() - 16, r.top() + 2, 14, 14), Qt::AlignCenter, QStringLiteral("✓"));
    }
}

void TileItem::setSelectedVisual(bool selected)
{
    if (m_selected == selected) {
        return;
    }
    m_selected = selected;
    update();
}

void TileItem::setHoveredVisual(bool hovered)
{
    if (m_hovered == hovered) {
        return;
    }
    m_hovered = hovered;
    update();
}

void TileItem::setOwnerColor(const QColor& color)
{
    setOwnerInfo(color.isValid() ? m_ownerId : -1, color);
}

void TileItem::setOwnerInfo(int ownerId, const QColor& color)
{
    m_ownerId = ownerId;
    m_ownerColor = color;
    update();
}

void TileItem::setShopType(ShopType type)
{
    setShopInfo(type, m_shopOwnerId, m_shopOwnerColor);
}

void TileItem::setShopInfo(ShopType type, int ownerId, const QColor& color)
{
    m_shopType = type;
    m_shopOwnerId = ownerId;
    m_shopOwnerColor = color;
    update();
}

void TileItem::setBuildable(bool buildable)
{
    if (m_buildable == buildable) {
        return;
    }
    m_buildable = buildable;
    update();
}

void TileItem::setTradeTarget(bool enabled)
{
    if (m_tradeTarget == enabled) {
        return;
    }
    m_tradeTarget = enabled;
    update();
}

void TileItem::setChoiceCandidate(bool enabled)
{
    if (m_choiceCandidate == enabled) {
        return;
    }
    m_choiceCandidate = enabled;
    update();
}

void TileItem::setChoiceSelected(bool enabled)
{
    if (m_choiceSelected == enabled) {
        return;
    }
    m_choiceSelected = enabled;
    update();
}

void TileItem::setChoiceBlinkOn(bool enabled)
{
    if (m_choiceBlinkOn == enabled) {
        return;
    }
    m_choiceBlinkOn = enabled;
    update();
}

void TileItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    emit clicked(m_tileId);
    QGraphicsObject::mousePressEvent(event);
}

void TileItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
    Q_UNUSED(event);
    setHoveredVisual(true);
}

void TileItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    Q_UNUSED(event);
    setHoveredVisual(false);
}
