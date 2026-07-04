#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QMap>
#include "core/GameState.h"

class GameController : public QObject {
    Q_OBJECT

public:
    explicit GameController(QObject* parent = nullptr);

    const GameState& state() const { return m_state; }
    GamePhase phase() const { return m_state.phase; }
    int currentPlayerId() const { return m_state.currentPlayerId(); }
    int localPlayerId() const { return m_state.localPlayerId; }
    int selectedTileId() const { return m_state.selectedTileId; }
    bool isFinalSettlementReady() const { return m_state.finalSettlementReady; }
    QString boardThemeKey() const { return m_state.boardThemeKey; }

    const Player* playerById(int playerId) const { return m_state.playerById(playerId); }
    const Tile* tileById(int tileId) const { return m_state.tileById(tileId); }
    const Shop* shopById(int shopId) const { return m_state.shopById(shopId); }
    QList<Player> players() const { return m_state.players; }
    QList<Tile> tiles() const { return m_state.tiles; }

    QList<int> adjacentTiles(int tileId) const;
    QList<int> buildableTilesForCurrentPlayer() const;
    QList<int> buildableTilesForPlayer(int playerId) const;
    bool playerHasBuildOptions(int playerId) const;

    bool canBuildShop(int playerId,
                      ShopType type,
                      const QList<int>& tileIds,
                      QString* reason = nullptr) const;

    bool validateTrade(const TradeProposal& proposal,
                       QString* reason = nullptr) const;

    int calculatePlayerIncome(int playerId) const;

    
    QJsonObject exportStateToJson() const;

public slots:
    void startLocalGame(int playerCount = 4, int localPlayerId = 1);
    void setAutoplayNonLocalPlayers(bool enabled);

    
    void nextPhase();
    void nextTurn();
    void drawTileForCurrentPlayer();

    
    void dealInitialTiles();

    void selectTile(int tileId);
    void clearSelectedTile();
    void setCurrentPlayer(int playerId);
    void setLocalPlayerId(int playerId);
    void setPlayerDisplayName(int playerId, const QString& displayName);
    void setBoardThemeKey(const QString& key);

    void submitTileChoices(int playerId, const QList<int>& selectedTileIds);
    void finishBuildChoicesForPlayer(int playerId);

    bool buildShop(int playerId,
                   ShopType type,
                   const QList<int>& tileIds,
                   QString* errorMessage = nullptr);

    bool applyTrade(const TradeProposal& proposal,
                    QString* errorMessage = nullptr);

    void applyIncomeForAllPlayers();

    void importStateFromJson(const QJsonObject& object, bool keepLocalPlayer = true);

    
    void markTradeFinishedForPlayer(int playerId);
    void finishTradeAndAdvanceRound();

signals:
    void stateChanged();
    void tileUpdated(int tileId);
    void playerUpdated(int playerId);
    void tileSelected(int tileId);
    void phaseChanged(GamePhase phase);
    void messageGenerated(const QString& message);

    
    void tileChoiceRequested(int playerId, QList<int> candidateTileIds, int chooseCount);

    
    void buildChoiceRequested(int playerId);

private:
    GameState m_state;

    static constexpr int kTileChoiceOptionsPerPlayer = 4; 
    static constexpr int kTilesPerPlayerPerRound = 2;     
    static constexpr int kShopCardsPerPlayerPerRound = 2;
    static constexpr int kMaxRounds = 6;
    static constexpr int kPhaseDelayMs = 1300;

    int m_tileChoicePlayerIndex = 0;
    int m_buildPlayerIndex = 0;
    QList<int> m_currentTileCandidates;
    QMap<int, QList<int>> m_pendingTileAwards;
    bool m_isAdvancingRound = false;
    QString m_selectedBoardThemeKey = QStringLiteral("porcelain");
    bool m_autoplayNonLocalPlayers = false;

    void createDefaultPlayers(int playerCount);
    void createDefaultBoard();

    void beginAutomaticRound();
    void requestTileChoiceForNextPlayer();
    bool shouldAutoplayPlayer(int playerId) const;
    void finishDrawTileStage();
    void autoDrawShopCardsForAllPlayers();

    void beginBuildStage();
    void requestBuildChoiceForNextPlayer();
    void beginIncomeStage();
    void beginRoundEndStage();

    QList<int> unownedTileIds() const;
    QList<int> emptyOwnedTileIds(int playerId) const;
    QList<int> randomTileCandidates(int count) const;
    ShopType randomAvailableShopType(const Player& player) const;
    ShopType randomShopTypeFromDeck() const;

    bool areTilesConnected(const QList<int>& tileIds) const;
    void transferAsset(int fromPlayerId, int toPlayerId, const TradeAsset& asset);
    void normalizeShopOwnership();
    int baseIncomeForShop(ShopType type) const;
};
