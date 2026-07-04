#include "ui/PlayerPanel.h"

#include "core/GameController.h"
#include "utils/AssetManager.h"

#include <QColor>
#include <QEvent>
#include <QFont>
#include <QGraphicsDropShadowEffect>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPointer>
#include <QSize>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <utility>

namespace {

QString playerAvatarKey(int playerId)
{
    switch (playerId) {
    case 1: return QStringLiteral("avatars/player1.png");
    case 2: return QStringLiteral("avatars/player2.png");
    case 3: return QStringLiteral("avatars/player3.png");
    case 4: return QStringLiteral("avatars/player4.png");
    case 5: return QStringLiteral("avatars/player5.png");
    default: return QStringLiteral("avatars/player1.png");
    }
}

QString roleNameByColor(const QColor& color)
{
    if (color == QColor(220, 70, 70) || color == QColor(196, 72, 72)) return QStringLiteral("茶楼女老板");
    if (color == QColor(220, 170, 60) || color == QColor(71, 113, 184)) return QStringLiteral("古董商人");
    if (color == QColor(245, 245, 245)) return QStringLiteral("票号掌柜");
    if (color == QColor(60, 170, 100) || color == QColor(86, 151, 86)) return QStringLiteral("药材铺掌柜");
    if (color == QColor(40, 40, 40)) return QStringLiteral("码头工头");
    if (color == QColor(150, 93, 181)) return QStringLiteral("粤剧名伶");
    return QStringLiteral("街区角色");
}

QString moneyTextForPlayer(const Player& player, int visiblePlayerId)
{
    return player.id == visiblePlayerId ? QString::number(player.money) : QStringLiteral("隐藏");
}

QPixmap circularAvatarPixmap(const QPixmap& source, const QColor& playerColor, bool current)
{
    const int size = 54;
    const int ringWidth = current ? 5 : 4;
    const int innerPad = ringWidth + 3;
    const QRect outerRect(1, 1, size - 2, size - 2);
    const QRect innerRect(innerPad, innerPad, size - innerPad * 2, size - innerPad * 2);

    QPixmap result(size, size);
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QColor ring = playerColor.isValid() ? playerColor : QColor(204, 166, 92);

    
    painter.setBrush(ring);
    painter.setPen(QPen(QColor(32, 20, 13, 235), 2));
    painter.drawEllipse(outerRect);

    painter.setBrush(QColor(244, 219, 174, 255));
    painter.setPen(QPen(current ? ring.lighter(185) : ring.darker(125), 2));
    painter.drawEllipse(outerRect.adjusted(ringWidth, ringWidth, -ringWidth, -ringWidth));

    QPixmap scaled = source.scaled(innerRect.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    const int sx = qMax(0, (scaled.width() - innerRect.width()) / 2);
    const int sy = qMax(0, (scaled.height() - innerRect.height()) / 2);
    scaled = scaled.copy(sx, sy, innerRect.width(), innerRect.height());

    QPainterPath clip;
    clip.addEllipse(innerRect);
    painter.setClipPath(clip);
    painter.fillPath(clip, QColor(244, 219, 174));
    painter.drawPixmap(innerRect.topLeft(), scaled);
    painter.setClipping(false);

    painter.setPen(QPen(QColor(255, 244, 204, current ? 210 : 145), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(innerRect.adjusted(1, 1, -1, -1));

    return result;
}

QString playerButtonStyle(const QColor& playerColor, bool current)
{
    const QColor base = playerColor.isValid() ? playerColor : QColor(208, 171, 98);
    const QColor border = current ? base.lighter(165) : base.lighter(120);
    const QColor hover = base.lighter(190);
    const QColor text = QColor(255, 238, 197);
    const int borderWidth = current ? 6 : 5;

    return QStringLiteral(
        "QPushButton {"
        " background: rgba(18, 12, 8, 20);"
        " color: %1;"
        " border: %2px solid %3;"
        " border-radius: 15px;"
        " padding: 5px 8px 5px 7px;"
        " text-align: left;"
        " font-size: 11px;"
        " font-weight: 650;"
        "}"
        "QPushButton:hover {"
        " background: rgba(255, 218, 142, 24);"
        " color: %1;"
        " border: %2px solid %4;"
        "}"
        "QPushButton:pressed {"
        " background: rgba(255, 218, 142, 34);"
        " color: %1;"
        " border: %2px solid %4;"
        "}"
    ).arg(text.name())
     .arg(borderWidth)
     .arg(border.name())
     .arg(hover.name());
}

QString tradeButtonStyle(const QColor& playerColor, bool enabled)
{
    const QColor base = playerColor.isValid() ? playerColor : QColor(208, 171, 98);
    const QColor border = enabled ? base.lighter(142) : QColor(118, 103, 82);
    const QColor text = enabled ? QColor(255, 236, 190) : QColor(162, 148, 124);

    return QStringLiteral(
        "QPushButton {"
        " background: rgba(20, 13, 8, 30);"
        " color: %1;"
        " border: 4px solid %2;"
        " border-radius: 13px;"
        " font-size: 12px;"
        " font-weight: 800;"
        " padding: 3px;"
        "}"
        "QPushButton:hover {"
        " background: rgba(255, 216, 138, 28);"
        " border-color: %3;"
        "}"
        "QPushButton:disabled {"
        " background: rgba(18, 12, 8, 18);"
        " color: %4;"
        " border-color: rgba(118, 103, 82, 185);"
        "}"
    ).arg(text.name())
     .arg(border.name())
     .arg(base.lighter(185).name())
     .arg(QColor(162, 148, 124).name());
}


const Player* findPlayerById(const QList<Player>& players, int playerId)
{
    for (const Player& player : players) {
        if (player.id == playerId) {
            return &player;
        }
    }
    return nullptr;
}

} 

PlayerPanel::PlayerPanel(QWidget* parent)
    : QWidget(parent)
    , m_layout(new QVBoxLayout(this))
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setStyleSheet(QStringLiteral("PlayerPanel { background: transparent; }"));

    m_layout->setContentsMargins(8, 4, 8, 4);
    m_layout->setSpacing(18);

    auto* title = new QLabel(QStringLiteral("玩家信息"), this);
    title->setAlignment(Qt::AlignCenter);
    title->setMinimumHeight(28);
    title->setStyleSheet(QStringLiteral(
        "QLabel {"
        " color: #ffe8ae;"
        " font-size: 15px;"
        " font-weight: 800;"
        " letter-spacing: 2px;"
        " background: rgba(22, 14, 8, 40);"
        " border: 4px solid rgba(220, 176, 99, 190);"
        " border-radius: 14px;"
        " padding: 4px 9px;"
        "}"));
    m_layout->addWidget(title);
}

void PlayerPanel::setController(GameController* controller)
{
    m_controller = controller;
}

void PlayerPanel::setPlayers(const QList<Player>& players)
{
    m_players = players;
    rebuild();
}

void PlayerPanel::setCurrentPlayer(int playerId)
{
    m_currentPlayerId = playerId;

    for (auto it = m_cardButtons.begin(); it != m_cardButtons.end(); ++it) {
        const bool current = it.key() == playerId;
        const Player* player = findPlayerById(m_players, it.key());
        const QColor color = player ? player->color : QColor();
        it.value()->setStyleSheet(playerButtonStyle(color, current));
        if (player) {
            it.value()->setText(playerText(*player));
            it.value()->setIcon(QIcon(circularAvatarPixmap(
                AssetManager::instance().pixmap(playerAvatarKey(player->id)),
                color.isValid() ? color : QColor(204, 166, 92),
                current)));
        }
    }

    for (auto it = m_tradeButtons.begin(); it != m_tradeButtons.end(); ++it) {
        const int playerId = it.key();
        QPushButton* button = it.value();
        const Player* player = findPlayerById(m_players, playerId);
        if (!button || !player) {
            continue;
        }
        const bool self = playerId == m_currentPlayerId;
        const bool inTrade = m_controller && m_controller->phase() == GamePhase::Trade;
        const bool enabled = inTrade && !self && player->active && !player->tradeFinishedThisRound;
        button->setText(self ? QStringLiteral("自己") : QStringLiteral("交易"));
        button->setEnabled(enabled);
        button->setToolTip(self ? QStringLiteral("不能和自己交易")
                                : (enabled ? QStringLiteral("向该玩家发起交易")
                                           : QStringLiteral("仅交易阶段且双方未结束交易时可用")));
        button->setStyleSheet(tradeButtonStyle(player->color, enabled));
    }
}

QString PlayerPanel::playerText(const Player& player) const
{
    const QString localMark = player.id == m_currentPlayerId ? QStringLiteral("  ◆ 当前") : QString();
    return QStringLiteral("%1%2\n%3｜资金 %4\n地块 %5   店铺 %6")
        .arg(player.name)
        .arg(localMark)
        .arg(roleNameByColor(player.color))
        .arg(moneyTextForPlayer(player, m_currentPlayerId))
        .arg(player.ownedTileIds.size())
        .arg(player.shops.size());
}


bool PlayerPanel::eventFilter(QObject* watched, QEvent* event)
{
    return QWidget::eventFilter(watched, event);
}

void PlayerPanel::setupHoverAnimation(QPushButton* button, const QColor& playerColor)
{
    Q_UNUSED(button)
    Q_UNUSED(playerColor)
    
    
}


void PlayerPanel::rebuild()
{
    while (m_layout->count() > 1) {
        QLayoutItem* item = m_layout->takeAt(1);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    m_cardButtons.clear();
    m_tradeButtons.clear();
    m_shadowEffects.clear();
    m_hoverAnimations.clear();

    for (const Player& player : std::as_const(m_players)) {
        auto* row = new QFrame(this);
        row->setObjectName(QStringLiteral("playerCardRow"));
        row->setAttribute(Qt::WA_TranslucentBackground, true);
        row->setStyleSheet(QStringLiteral("QFrame#playerCardRow { background: transparent; border: none; }"));
        row->setMinimumHeight(78);
        row->setMaximumHeight(84);
        row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(8);

        auto* card = new QPushButton(playerText(player), row);
        card->setMinimumHeight(74);
        card->setMaximumHeight(80);
        card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        card->setIcon(QIcon(circularAvatarPixmap(
            AssetManager::instance().pixmap(playerAvatarKey(player.id)),
            player.color.isValid() ? player.color : QColor(204, 166, 92),
            player.id == m_currentPlayerId)));
        card->setIconSize(QSize(54, 54));
        card->setStyleSheet(playerButtonStyle(player.color, player.id == m_currentPlayerId));
        card->setCursor(Qt::PointingHandCursor);
        connect(card, &QPushButton::clicked, this, [this, id = player.id] {
            emit playerClicked(id);
        });
        setupHoverAnimation(card, player.color.isValid() ? player.color : QColor(194, 161, 98));

        auto* tradeButton = new QPushButton(row);
        tradeButton->setFixedSize(56, 56);
        tradeButton->setCursor(Qt::PointingHandCursor);
        connect(tradeButton, &QPushButton::clicked, this, [this, id = player.id] {
            emit tradeWithPlayerRequested(id);
        });

        rowLayout->addWidget(card, 1);
        rowLayout->addWidget(tradeButton, 0, Qt::AlignVCenter);

        m_layout->addWidget(row);
        m_cardButtons.insert(player.id, card);
        m_tradeButtons.insert(player.id, tradeButton);
    }

    m_layout->addStretch(1);
    setCurrentPlayer(m_currentPlayerId);
}

void PlayerPanel::refreshPlayer(int playerId)
{
    if (!m_controller) {
        return;
    }

    const Player* player = m_controller->playerById(playerId);
    QPushButton* button = m_cardButtons.value(playerId, nullptr);
    if (!player || !button) {
        return;
    }

    button->setText(playerText(*player));
    button->setIcon(QIcon(circularAvatarPixmap(
        AssetManager::instance().pixmap(playerAvatarKey(player->id)),
        player->color.isValid() ? player->color : QColor(204, 166, 92),
        player->id == m_currentPlayerId)));
    setCurrentPlayer(m_currentPlayerId);
}
