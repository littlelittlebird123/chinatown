#pragma once

#include <QPushButton>
#include <QWidget>
#include "core/GameTypes.h"

class ActionPanel : public QWidget {
    Q_OBJECT

public:
    explicit ActionPanel(QWidget* parent = nullptr);

    void setPhase(GamePhase phase);
    void setTradeClosedForLocalPlayer(bool closed);
    void setButtonHint(const QString& buttonName, const QString& hint);

signals:
    
    void drawTileRequested();
    void tradeRequested();
    void buildRequested();
    void incomeRequested();
    void nextPhaseRequested();

private:
    void refreshButtons();

    GamePhase m_phase = GamePhase::DrawTile;
    bool m_tradeClosedForLocalPlayer = false;
    QPushButton* m_tradeButton = nullptr;
    QPushButton* m_buildButton = nullptr;
};
