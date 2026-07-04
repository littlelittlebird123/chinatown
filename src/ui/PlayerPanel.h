#pragma once

#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QHash>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <QWidget>

#include "core/GameTypes.h"

class GameController;

class PlayerPanel : public QWidget {
    Q_OBJECT

public:
    explicit PlayerPanel(QWidget* parent = nullptr);

    void setController(GameController* controller);
    void setPlayers(const QList<Player>& players);
    void setCurrentPlayer(int playerId);

public slots:
    void refreshPlayer(int playerId);

signals:
    void playerClicked(int playerId);
    void tradeWithPlayerRequested(int playerId);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void rebuild();
    void setupHoverAnimation(QPushButton* button, const QColor& playerColor);
    QString playerText(const Player& player) const;

    GameController* m_controller = nullptr;
    QList<Player> m_players;
    int m_currentPlayerId = -1;
    QVBoxLayout* m_layout = nullptr;
    QHash<int, QPushButton*> m_cardButtons;
    QHash<int, QPushButton*> m_tradeButtons;
    QHash<QPushButton*, QGraphicsDropShadowEffect*> m_shadowEffects;
    QHash<QPushButton*, QVariantAnimation*> m_hoverAnimations;
};
