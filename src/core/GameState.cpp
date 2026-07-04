#include "core/GameState.h"

void GameState::clear()
{
    players.clear();
    tiles.clear();
    shops.clear();
    shopDeckRemaining.clear();
    currentPlayerIndex = 0;
    localPlayerId = 1;
    phase = GamePhase::DrawTile;
    round = 1;
    selectedTileId = -1;
    nextShopId = 1;
    finalSettlementReady = false;
    boardThemeKey = QStringLiteral("porcelain");
}

Player* GameState::playerById(int playerId)
{
    for (Player& p : players) {
        if (p.id == playerId) {
            return &p;
        }
    }
    return nullptr;
}

const Player* GameState::playerById(int playerId) const
{
    for (const Player& p : players) {
        if (p.id == playerId) {
            return &p;
        }
    }
    return nullptr;
}

Tile* GameState::tileById(int tileId)
{
    for (Tile& t : tiles) {
        if (t.id == tileId) {
            return &t;
        }
    }
    return nullptr;
}

const Tile* GameState::tileById(int tileId) const
{
    for (const Tile& t : tiles) {
        if (t.id == tileId) {
            return &t;
        }
    }
    return nullptr;
}

Shop* GameState::shopById(int shopId)
{
    for (Shop& s : shops) {
        if (s.id == shopId) {
            return &s;
        }
    }
    return nullptr;
}

const Shop* GameState::shopById(int shopId) const
{
    for (const Shop& s : shops) {
        if (s.id == shopId) {
            return &s;
        }
    }
    return nullptr;
}

Player* GameState::currentPlayer()
{
    if (players.isEmpty() || currentPlayerIndex < 0 || currentPlayerIndex >= players.size()) {
        return playerById(localPlayerId);
    }
    return &players[currentPlayerIndex];
}

const Player* GameState::currentPlayer() const
{
    if (players.isEmpty() || currentPlayerIndex < 0 || currentPlayerIndex >= players.size()) {
        return playerById(localPlayerId);
    }
    return &players[currentPlayerIndex];
}

int GameState::currentPlayerId() const
{
    const Player* p = currentPlayer();
    return p ? p->id : -1;
}
