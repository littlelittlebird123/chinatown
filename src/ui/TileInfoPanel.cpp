#include "ui/TileInfoPanel.h"

#include "core/GameController.h"

#include <QVBoxLayout>

TileInfoPanel::TileInfoPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    m_titleLabel = new QLabel(QStringLiteral("地块信息"), this);
    m_titleLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 15px;"));
    layout->addWidget(m_titleLabel);

    m_infoLabel = new QLabel(this);
    m_infoLabel->setWordWrap(true);
    m_infoLabel->hide();
    layout->addWidget(m_infoLabel, 1);

    clear();
}

void TileInfoPanel::setController(GameController* controller)
{
    m_controller = controller;
}

void TileInfoPanel::showTile(int tileId)
{
    m_tileId = tileId;
    if (!m_controller || tileId < 0) {
        clear();
        return;
    }

    const Tile* tile = m_controller->tileById(tileId);
    if (!tile) {
        clear();
        return;
    }

    const Player* owner = m_controller->playerById(tile->ownerId);
    const QString ownerName = owner ? owner->name : QStringLiteral("无");

    QString shopText = QStringLiteral("无");
    if (tile->shopType != ShopType::None) {
        shopText = shopTypeName(tile->shopType);
    }

    m_titleLabel->setText(QStringLiteral("地块 %1").arg(tileId));
    m_infoLabel->setText(QStringLiteral("主人：%1\n商铺：%2").arg(ownerName, shopText));
    m_infoLabel->show();
}

void TileInfoPanel::clear()
{
    m_tileId = -1;
    m_titleLabel->setText(QStringLiteral("地块信息"));
    m_infoLabel->clear();
    m_infoLabel->hide();
}
