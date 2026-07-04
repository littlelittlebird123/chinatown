#include "ui/HandPanel.h"

#include "core/GameController.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSet>
#include <algorithm>

HandPanel::HandPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 6, 8, 6);

    auto* title = new QLabel(QStringLiteral("本机玩家资源：未建设店铺卡"), this);
    title->setStyleSheet(QStringLiteral("font-weight: bold;"));
    root->addWidget(title);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_container = new QWidget(m_scrollArea);
    m_cardLayout = new QHBoxLayout(m_container);
    m_cardLayout->setContentsMargins(4, 4, 4, 4);
    m_cardLayout->setSpacing(6);
    m_cardLayout->addStretch(1);

    m_scrollArea->setWidget(m_container);
    root->addWidget(m_scrollArea);
}

void HandPanel::setController(GameController* controller)
{
    m_controller = controller;
}

void HandPanel::setFilter(AssetFilter filter)
{
    m_filter = filter;
    refreshForPlayer(m_playerId);
}

void HandPanel::sortCards(AssetSortMode mode)
{
    m_sortMode = mode;
    refreshForPlayer(m_playerId);
}

void HandPanel::refreshForPlayer(int playerId)
{
    m_playerId = playerId;

    QList<CardInfo> cards;
    if (!m_controller) {
        rebuildCards(cards);
        return;
    }

    const Player* player = m_controller->playerById(playerId);
    if (!player) {
        rebuildCards(cards);
        return;
    }

    QSet<int> buildable;
    if (playerId == m_controller->currentPlayerId()) {
        const QList<int> ids = m_controller->buildableTilesForCurrentPlayer();
        for (int id : ids) {
            buildable.insert(id);
        }
    }

    Q_UNUSED(buildable);

    
    
    if (m_filter == AssetFilter::All ||
        m_filter == AssetFilter::ShopOnly ||
        m_filter == AssetFilter::TradeableOnly ||
        m_filter == AssetFilter::BuildableOnly) {
        for (ShopType type : allShopTypes()) {
            const int count = player->shopCards.value(type, 0);
            if (count <= 0) {
                continue;
            }

            CardInfo card;
            card.asset = { TradeAssetType::ShopCard, static_cast<int>(type), type };
            card.value = static_cast<int>(type) * 100;
            card.text = QStringLiteral("%1卡 ×%2\n最高规模 %3")
                    .arg(shopTypeName(type))
                    .arg(count)
                    .arg(shopMaxSize(type));
            cards.append(card);
        }
    }

    std::sort(cards.begin(), cards.end(), [this](const CardInfo& a, const CardInfo& b) {
        switch (m_sortMode) {
        case AssetSortMode::ByType:
            return static_cast<int>(a.asset.type) < static_cast<int>(b.asset.type);
        case AssetSortMode::ByValue:
            return a.value < b.value;
        case AssetSortMode::ByShopType:
            return static_cast<int>(a.asset.shopType) < static_cast<int>(b.asset.shopType);
        case AssetSortMode::ById:
        default:
            return a.asset.id < b.asset.id;
        }
    });

    rebuildCards(cards);
}

void HandPanel::rebuildCards(const QList<CardInfo>& cards)
{
    while (m_cardLayout->count() > 0) {
        QLayoutItem* item = m_cardLayout->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    if (cards.isEmpty()) {
        auto* empty = new QLabel(QStringLiteral("暂无未建设店铺卡"), m_container);
        empty->setStyleSheet(QStringLiteral("color: #f7e6bd; padding: 8px;"));
        m_cardLayout->addWidget(empty);
    }

    for (const CardInfo& card : cards) {
        auto* button = new QPushButton(card.text, m_container);
        button->setFixedSize(118, 74);
        button->setStyleSheet(QStringLiteral("QPushButton { background: #fbf3dc; color: #3d3329; border: 1px solid #8f7d62; border-radius: 6px; font-weight: bold; }"));
        if (card.asset.type == TradeAssetType::Tile) {
            connect(button, &QPushButton::clicked, this, [this, id = card.asset.id] {
                emit tileCardClicked(id);
            });
        }
        m_cardLayout->addWidget(button);
    }

    m_cardLayout->addStretch(1);
}
