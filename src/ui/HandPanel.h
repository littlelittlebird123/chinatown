#pragma once

#include <QHBoxLayout>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

#include "core/GameTypes.h"

class GameController;

class HandPanel : public QWidget {
    Q_OBJECT

public:
    explicit HandPanel(QWidget* parent = nullptr);

    void setController(GameController* controller);
    void setFilter(AssetFilter filter);
    void sortCards(AssetSortMode mode);

public slots:
    void refreshForPlayer(int playerId);

signals:
    void tileCardClicked(int tileId);

private:
    struct CardInfo {
        QString text;
        TradeAsset asset;
        int value = 0;
    };

    void rebuildCards(const QList<CardInfo>& cards);

    GameController* m_controller = nullptr;
    int m_playerId = -1;
    AssetFilter m_filter = AssetFilter::All;
    AssetSortMode m_sortMode = AssetSortMode::ById;

    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_container = nullptr;
    QHBoxLayout* m_cardLayout = nullptr;
};
