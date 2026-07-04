#pragma once

#include "core/GameTypes.h"

class GameState {
public:
    void clear();

    Player* playerById(int playerId);
    const Player* playerById(int playerId) const;

    Tile* tileById(int tileId);
    const Tile* tileById(int tileId) const;

    Shop* shopById(int shopId);
    const Shop* shopById(int shopId) const;

    Player* currentPlayer();
    const Player* currentPlayer() const;
    int currentPlayerId() const;

    QList<Player> players;
    QList<Tile> tiles;
    QList<Shop> shops;
    QMap<ShopType, int> shopDeckRemaining;

    int currentPlayerIndex = 0;
    int localPlayerId = 1;
    GamePhase phase = GamePhase::DrawTile;
    int round = 1;
    int selectedTileId = -1;
    int nextShopId = 1;
    bool finalSettlementReady = false;
    QString boardThemeKey = QStringLiteral("porcelain");
};
