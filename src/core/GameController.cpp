#include "core/GameController.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QQueue>
#include <QRandomGenerator>
#include <QSet>
#include <QTimer>
#include <algorithm>
#include <limits>
#include <utility>

namespace {

constexpr int kOfficialMaxRounds = 6;
constexpr int kInitialMoney = 50000;
constexpr int kPhaseDelayMsLocal = 450;

struct BuildingDealRule {
    int dealCount = 0;
    int keepCount = 0;
};

BuildingDealRule dealRuleFor(int playerCount, int round)
{
    round = std::clamp(round, 1, kOfficialMaxRounds);

    if (playerCount <= 3) {
        return round == 1 ? BuildingDealRule{7, 5} : BuildingDealRule{6, 4};
    }

    if (playerCount == 4) {
        return round == 1 ? BuildingDealRule{6, 4} : BuildingDealRule{5, 3};
    }

    return round <= 3 ? BuildingDealRule{5, 3} : BuildingDealRule{4, 2};
}

int yearForRound(int round)
{
    return 1964 + std::clamp(round, 1, kOfficialMaxRounds);
}

int maxSizeForShop(ShopType type)
{
    
    
    return shopMaxSize(type);
}

int incompleteIncomeForSize(int size)
{
    switch (size) {
    case 1: return 10000;
    case 2: return 20000;
    case 3: return 40000;
    case 4: return 60000;
    case 5: return 80000;
    default: return 0;
    }
}

int completeIncomeForMaxSize(int maxSize)
{
    switch (maxSize) {
    case 3: return 50000;
    case 4: return 80000;
    case 5: return 110000;
    case 6: return 140000;
    default: return 0;
    }
}

int incomeForConnectedGroup(int groupSize, int maxSize)
{
    if (groupSize <= 0 || maxSize <= 0) {
        return 0;
    }

    const int completeGroups = groupSize / maxSize;
    const int remainder = groupSize % maxSize;
    return completeGroups * completeIncomeForMaxSize(maxSize) + incompleteIncomeForSize(remainder);
}

bool shopOccupiesTile(const Shop& shop, int tileId)
{
    return shop.tileIds.contains(tileId);
}

QString finalScoreMessage(const GameState& state)
{
    int bestMoney = std::numeric_limits<int>::min();
    int bestShopCount = -1;
    QStringList winners;

    for (const Player& player : state.players) {
        int shopCount = 0;
        for (const Tile& tile : state.tiles) {
            if (tile.shopOwnerId == player.id && tile.shopType != ShopType::None) {
                ++shopCount;
            }
        }

        if (player.money > bestMoney || (player.money == bestMoney && shopCount > bestShopCount)) {
            bestMoney = player.money;
            bestShopCount = shopCount;
            winners.clear();
            winners << player.name;
        } else if (player.money == bestMoney && shopCount == bestShopCount) {
            winners << player.name;
        }
    }

    return QStringLiteral("最终结算：胜利者：%1。判定顺序为资金最多，资金相同则比较图板上已建设店铺数量。")
        .arg(winners.isEmpty() ? QStringLiteral("无") : winners.join(QStringLiteral("、")));
}

QJsonArray intListToJsonArray(const QList<int>& values)
{
    QJsonArray array;
    for (int value : values) {
        array.append(value);
    }
    return array;
}

QList<int> intListFromJsonArray(const QJsonArray& array)
{
    QList<int> values;
    for (const QJsonValue& value : array) {
        values.append(value.toInt());
    }
    return values;
}

QJsonObject shopCardsToJsonObject(const QMap<ShopType, int>& cards)
{
    QJsonObject object;
    for (auto it = cards.constBegin(); it != cards.constEnd(); ++it) {
        if (it.value() != 0) {
            object[QString::number(static_cast<int>(it.key()))] = it.value();
        }
    }
    return object;
}

QMap<ShopType, int> shopCardsFromJsonObject(const QJsonObject& object)
{
    QMap<ShopType, int> cards;
    for (ShopType type : allShopTypes()) {
        cards[type] = 0;
    }
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        bool ok = false;
        const int key = it.key().toInt(&ok);
        if (ok) {
            cards[static_cast<ShopType>(key)] = it.value().toInt();
        }
    }
    return cards;
}

QJsonObject tileToJson(const Tile& tile)
{
    QJsonObject object;
    object[QStringLiteral("id")] = tile.id;
    object[QStringLiteral("x")] = tile.gridPos.x();
    object[QStringLiteral("y")] = tile.gridPos.y();
    object[QStringLiteral("ownerId")] = tile.ownerId;
    object[QStringLiteral("shopType")] = static_cast<int>(tile.shopType);
    object[QStringLiteral("shopOwnerId")] = tile.shopOwnerId;
    object[QStringLiteral("adjacentIds")] = intListToJsonArray(tile.adjacentIds);
    object[QStringLiteral("locked")] = tile.locked;
    object[QStringLiteral("baseValue")] = tile.baseValue;
    object[QStringLiteral("district")] = tile.district;
    return object;
}

Tile tileFromJson(const QJsonObject& object)
{
    Tile tile;
    tile.id = object.value(QStringLiteral("id")).toInt(-1);
    tile.gridPos = QPoint(object.value(QStringLiteral("x")).toInt(),
                          object.value(QStringLiteral("y")).toInt());
    tile.ownerId = object.value(QStringLiteral("ownerId")).toInt(-1);
    tile.shopType = static_cast<ShopType>(object.value(QStringLiteral("shopType")).toInt(static_cast<int>(ShopType::None)));
    tile.shopOwnerId = object.value(QStringLiteral("shopOwnerId")).toInt(-1);
    tile.adjacentIds = intListFromJsonArray(object.value(QStringLiteral("adjacentIds")).toArray());
    tile.locked = object.value(QStringLiteral("locked")).toBool(false);
    tile.baseValue = object.value(QStringLiteral("baseValue")).toInt(1000);
    tile.district = object.value(QStringLiteral("district")).toInt(0);
    return tile;
}

QJsonObject shopToJson(const Shop& shop)
{
    QJsonObject object;
    object[QStringLiteral("id")] = shop.id;
    object[QStringLiteral("ownerId")] = shop.ownerId;
    object[QStringLiteral("type")] = static_cast<int>(shop.type);
    object[QStringLiteral("tileIds")] = intListToJsonArray(shop.tileIds);
    return object;
}

Shop shopFromJson(const QJsonObject& object)
{
    Shop shop;
    shop.id = object.value(QStringLiteral("id")).toInt(-1);
    shop.ownerId = object.value(QStringLiteral("ownerId")).toInt(-1);
    shop.type = static_cast<ShopType>(object.value(QStringLiteral("type")).toInt(static_cast<int>(ShopType::None)));
    shop.tileIds = intListFromJsonArray(object.value(QStringLiteral("tileIds")).toArray());
    return shop;
}

bool loadBoardLayoutFromJsonObject(const QJsonObject& root, QList<Tile>* outTiles)
{
    if (!outTiles) {
        return false;
    }

    const QJsonArray tilesArray = root.value(QStringLiteral("tiles")).toArray();
    if (tilesArray.isEmpty()) {
        return false;
    }

    QList<Tile> tiles;
    QSet<int> ids;
    for (const QJsonValue& value : tilesArray) {
        const QJsonObject object = value.toObject();
        Tile tile = tileFromJson(object);
        if (tile.id <= 0 || ids.contains(tile.id)) {
            return false;
        }
        ids.insert(tile.id);
        if (tile.adjacentIds.isEmpty()) {
            tile.adjacentIds = intListFromJsonArray(object.value(QStringLiteral("adjacent")).toArray());
        }
        tiles.append(tile);
    }

    *outTiles = tiles;
    return !outTiles->isEmpty();
}

bool loadBoardLayoutFromJsonFile(const QString& path, QList<Tile>* outTiles)
{
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return false;
    }

    return loadBoardLayoutFromJsonObject(document.object(), outTiles);
}

bool tryLoadBoardLayout(QList<Tile>* outTiles)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList paths = {
        QStringLiteral(":/maps/chinatown_85.json"),
        QStringLiteral("resources/maps/chinatown_85.json"),
        QStringLiteral("maps/chinatown_85.json"),
        appDir + QStringLiteral("/resources/maps/chinatown_85.json"),
        appDir + QStringLiteral("/maps/chinatown_85.json")
    };

    for (const QString& path : paths) {
        if (loadBoardLayoutFromJsonFile(path, outTiles)) {
            return true;
        }
    }
    return false;
}

QMap<ShopType, int> defaultShopDeck()
{
    QMap<ShopType, int> deck;
    for (ShopType type : allShopTypes()) {
        deck[type] = shopCardDeckCount(type);
    }
    return deck;
}

QMap<ShopType, int> reconstructShopDeck(const QList<Player>& players, const QList<Shop>& shops)
{
    QMap<ShopType, int> deck = defaultShopDeck();
    for (const Player& player : players) {
        for (auto it = player.shopCards.constBegin(); it != player.shopCards.constEnd(); ++it) {
            deck[it.key()] = std::max(0, deck.value(it.key(), 0) - it.value());
        }
    }
    for (const Shop& shop : shops) {
        if (shop.type != ShopType::None) {
            deck[shop.type] = std::max(0, deck.value(shop.type, 0) - 1);
        }
    }
    return deck;
}

} 

GameController::GameController(QObject* parent)
    : QObject(parent)
{
}

void GameController::startLocalGame(int playerCount, int localPlayerId)
{
    playerCount = std::clamp(playerCount, 3, 5);
    localPlayerId = std::clamp(localPlayerId, 1, playerCount);

    const QString selectedBoardThemeKey = m_selectedBoardThemeKey.trimmed().isEmpty()
        ? QStringLiteral("porcelain")
        : m_selectedBoardThemeKey;

    m_state.clear();
    m_state.boardThemeKey = selectedBoardThemeKey;
    createDefaultPlayers(playerCount);
    createDefaultBoard();
    m_state.shopDeckRemaining = defaultShopDeck();
    m_state.round = 1;
    m_state.currentPlayerIndex = localPlayerId - 1;
    m_state.localPlayerId = localPlayerId;
    m_tileChoicePlayerIndex = 0;
    m_buildPlayerIndex = 0;
    m_currentTileCandidates.clear();
    m_pendingTileAwards.clear();
    m_isAdvancingRound = false;

    emit messageGenerated(QStringLiteral("房间/本地棋局已初始化：本机绑定 玩家%1。请在房间创建完成、玩家都进入后再开始抽地块。")
                          .arg(localPlayerId));

    for (const Tile& tile : m_state.tiles) {
        emit tileUpdated(tile.id);
    }
    for (const Player& player : m_state.players) {
        emit playerUpdated(player.id);
    }

    emit phaseChanged(m_state.phase);
    emit stateChanged();
}

void GameController::setAutoplayNonLocalPlayers(bool enabled)
{
    m_autoplayNonLocalPlayers = enabled;
}

bool GameController::shouldAutoplayPlayer(int playerId) const
{
    return m_autoplayNonLocalPlayers
        && playerId > 0
        && playerId != m_state.localPlayerId;
}

void GameController::setBoardThemeKey(const QString& key)
{
    const QString cleaned = key.trimmed().isEmpty() ? QStringLiteral("porcelain") : key.trimmed();
    if (m_state.boardThemeKey == cleaned && m_selectedBoardThemeKey == cleaned) {
        return;
    }

    m_selectedBoardThemeKey = cleaned;
    m_state.boardThemeKey = cleaned;
    emit stateChanged();
}

namespace {

QColor defaultPlayerColorById(int playerId)
{
    switch (playerId) {
    case 1: return QColor(220, 70, 70);     
    case 2: return QColor(220, 170, 60);    
    case 3: return QColor(0, 0, 0);         
    case 4: return QColor(60, 170, 100);    
    case 5: return QColor(132, 86, 188);    
    default: return QColor(120, 120, 120);
    }
}

void normalizePlayerColors(QList<Player>& players)
{
    for (Player& player : players) {
        if (player.id == 3 || !player.color.isValid()) {
            player.color = defaultPlayerColorById(player.id);
        }
        
        if (player.id == 5 && player.color == QColor(40, 40, 40)) {
            player.color = defaultPlayerColorById(5);
        }
    }
}

} 

void GameController::createDefaultPlayers(int playerCount)
{
    for (int i = 0; i < playerCount; ++i) {
        Player player;
        player.id = i + 1;
        player.name = QStringLiteral("玩家%1").arg(i + 1);
        player.color = defaultPlayerColorById(player.id);
        player.money = kInitialMoney;
        player.active = true;

        for (ShopType type : allShopTypes()) {
            player.shopCards[type] = 0;
        }

        m_state.players.append(player);
    }
}

void GameController::createDefaultBoard()
{
    m_state.tiles.clear();

    QList<Tile> importedTiles;
    if (tryLoadBoardLayout(&importedTiles)) {
        m_state.tiles = importedTiles;
        emit messageGenerated(QStringLiteral("已导入地图文件：chinatown_85.json，共 %1 个地块。").arg(m_state.tiles.size()));
        return;
    }

    
    constexpr int columns = 17;
    constexpr int rows = 5;

    int id = 1;
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < columns; ++x) {
            Tile tile;
            tile.id = id++;
            tile.gridPos = QPoint(x, y);
            tile.baseValue = 1000;
            tile.district = y + 1;
            m_state.tiles.append(tile);
        }
    }

    for (Tile& tile : m_state.tiles) {
        const int x = tile.gridPos.x();
        const int y = tile.gridPos.y();

        auto tileIdAt = [](int tx, int ty) -> int {
            if (tx < 0 || tx >= columns || ty < 0 || ty >= rows) {
                return -1;
            }
            return ty * columns + tx + 1;
        };

        const QList<int> neighbours = {
            tileIdAt(x - 1, y),
            tileIdAt(x + 1, y),
            tileIdAt(x, y - 1),
            tileIdAt(x, y + 1)
        };

        for (int neighbourId : neighbours) {
            if (neighbourId > 0) {
                tile.adjacentIds.append(neighbourId);
            }
        }
    }

    emit messageGenerated(QStringLiteral("未找到地图 JSON，已使用 85 格兜底网格地图。"));
}

void GameController::dealInitialTiles()
{
    beginAutomaticRound();
}

void GameController::beginAutomaticRound()
{
    if (m_state.players.isEmpty()) {
        return;
    }

    clearSelectedTile();
    m_isAdvancingRound = false;
    m_tileChoicePlayerIndex = 0;
    m_currentTileCandidates.clear();
    m_pendingTileAwards.clear();

    m_state.finalSettlementReady = false;
    m_state.phase = GamePhase::DrawTile;
    const BuildingDealRule rule = dealRuleFor(m_state.players.size(), m_state.round);

    emit phaseChanged(m_state.phase);
    emit messageGenerated(QStringLiteral("第 %1 轮（%2 年）：进入抽地块阶段。每名玩家抽 %3 张地块，保留 %4 张。")
                          .arg(m_state.round)
                          .arg(yearForRound(m_state.round))
                          .arg(rule.dealCount)
                          .arg(rule.keepCount));
    emit stateChanged();

    QTimer::singleShot(kPhaseDelayMsLocal, this, &GameController::requestTileChoiceForNextPlayer);
}

QList<int> GameController::unownedTileIds() const
{
    QSet<int> reservedPendingIds;
    for (auto it = m_pendingTileAwards.constBegin(); it != m_pendingTileAwards.constEnd(); ++it) {
        for (int tileId : it.value()) {
            reservedPendingIds.insert(tileId);
        }
    }

    QList<int> ids;
    for (const Tile& tile : m_state.tiles) {
        if (tile.ownerId < 0 && !reservedPendingIds.contains(tile.id)) {
            ids.append(tile.id);
        }
    }
    return ids;
}

QList<int> GameController::emptyOwnedTileIds(int playerId) const
{
    QList<int> ids;
    for (const Tile& tile : m_state.tiles) {
        if (tile.ownerId == playerId && tile.shopType == ShopType::None && !tile.locked) {
            ids.append(tile.id);
        }
    }
    return ids;
}

QList<int> GameController::randomTileCandidates(int count) const
{
    QList<int> pool = unownedTileIds();
    QList<int> candidates;

    while (!pool.isEmpty() && candidates.size() < count) {
        const int index = QRandomGenerator::global()->bounded(int(pool.size()));
        candidates.append(pool.takeAt(index));
    }

    return candidates;
}

void GameController::requestTileChoiceForNextPlayer()
{
    if (m_state.phase != GamePhase::DrawTile) {
        return;
    }

    while (m_tileChoicePlayerIndex < m_state.players.size() &&
           !m_state.players[m_tileChoicePlayerIndex].active) {
        ++m_tileChoicePlayerIndex;
    }

    if (m_tileChoicePlayerIndex >= m_state.players.size()) {
        finishDrawTileStage();
        return;
    }

    m_state.currentPlayerIndex = m_tileChoicePlayerIndex;
    Player& player = m_state.players[m_tileChoicePlayerIndex];
    const BuildingDealRule rule = dealRuleFor(m_state.players.size(), m_state.round);
    m_currentTileCandidates = randomTileCandidates(rule.dealCount);

    if (m_currentTileCandidates.isEmpty()) {
        emit messageGenerated(QStringLiteral("地块牌堆已空，跳过剩余抽地块步骤。"));
        finishDrawTileStage();
        return;
    }

    const int chooseCount = std::min(rule.keepCount, int(m_currentTileCandidates.size()));
    emit messageGenerated(QStringLiteral("轮到 %1 抽地块：请从 %2 个候选中保留 %3 个。")
                          .arg(player.name)
                          .arg(m_currentTileCandidates.size())
                          .arg(chooseCount));

    if (shouldAutoplayPlayer(player.id)) {
        QList<int> autoSelected;
        for (int i = 0; i < chooseCount && i < m_currentTileCandidates.size(); ++i) {
            autoSelected.append(m_currentTileCandidates.at(i));
        }

        emit messageGenerated(QStringLiteral("%1 为单机自动玩家，已自动完成抽地块；结果将在所有玩家都抽完后统一揭示。").arg(player.name));

        const int autoPlayerId = player.id;
        QTimer::singleShot(kPhaseDelayMsLocal, this, [this, autoPlayerId, autoSelected] {
            submitTileChoices(autoPlayerId, autoSelected);
        });
        return;
    }

    emit tileChoiceRequested(player.id, m_currentTileCandidates, chooseCount);
}

void GameController::submitTileChoices(int playerId, const QList<int>& selectedTileIds)
{
    if (m_state.phase != GamePhase::DrawTile) {
        emit messageGenerated(QStringLiteral("当前不是抽地块阶段。"));
        return;
    }

    if (m_state.players.isEmpty()) {
        return;
    }

    Player* player = m_state.playerById(playerId);
    if (!player) {
        emit messageGenerated(QStringLiteral("找不到该玩家，不能选择地块。"));
        return;
    }

    if (m_tileChoicePlayerIndex < 0 || m_tileChoicePlayerIndex >= m_state.players.size() ||
        m_state.players[m_tileChoicePlayerIndex].id != player->id) {
        for (int i = 0; i < m_state.players.size(); ++i) {
            if (m_state.players[i].id == player->id) {
                m_tileChoicePlayerIndex = i;
                m_state.currentPlayerIndex = i;
                break;
            }
        }
    }

    if (m_tileChoicePlayerIndex < 0 || m_tileChoicePlayerIndex >= m_state.players.size() ||
        m_state.players[m_tileChoicePlayerIndex].id != player->id) {
        emit messageGenerated(QStringLiteral("当前不是该玩家选择地块。"));
        return;
    }

    if (m_currentTileCandidates.isEmpty() && !selectedTileIds.isEmpty()) {
        m_currentTileCandidates = selectedTileIds;
    }

    QSet<int> alreadyReservedByOthers;
    for (auto it = m_pendingTileAwards.constBegin(); it != m_pendingTileAwards.constEnd(); ++it) {
        if (it.key() == player->id) {
            continue;
        }
        for (int reservedTileId : it.value()) {
            alreadyReservedByOthers.insert(reservedTileId);
        }
    }

    const BuildingDealRule rule = dealRuleFor(m_state.players.size(), m_state.round);
    const int chooseCount = std::min(rule.keepCount, int(m_currentTileCandidates.size()));

    QList<int> normalized;
    for (int tileId : selectedTileIds) {
        const Tile* tile = m_state.tileById(tileId);
        if (tile &&
            tile->ownerId < 0 &&
            !alreadyReservedByOthers.contains(tileId) &&
            m_currentTileCandidates.contains(tileId) &&
            !normalized.contains(tileId)) {
            normalized.append(tileId);
        }
        if (normalized.size() >= chooseCount) {
            break;
        }
    }

    for (int tileId : std::as_const(m_currentTileCandidates)) {
        if (normalized.size() >= chooseCount) {
            break;
        }
        const Tile* tile = m_state.tileById(tileId);
        if (tile &&
            tile->ownerId < 0 &&
            !alreadyReservedByOthers.contains(tileId) &&
            !normalized.contains(tileId)) {
            normalized.append(tileId);
        }
    }

    QList<int> fallbackPool = unownedTileIds();
    for (int tileId : fallbackPool) {
        if (normalized.size() >= chooseCount) {
            break;
        }
        if (!normalized.contains(tileId)) {
            normalized.append(tileId);
        }
    }

    m_pendingTileAwards[player->id] = normalized;

    emit messageGenerated(QStringLiteral("%1 已完成抽地块选择；结果将在所有玩家全部抽完后统一显示到地图上。")
                          .arg(player->name));
    emit stateChanged();

    ++m_tileChoicePlayerIndex;
    m_currentTileCandidates.clear();
    if (!m_state.players.isEmpty()) {
        m_state.currentPlayerIndex = std::min(m_tileChoicePlayerIndex, int(m_state.players.size()) - 1);
    }
    QTimer::singleShot(kPhaseDelayMsLocal, this, &GameController::requestTileChoiceForNextPlayer);
}

void GameController::finishDrawTileStage()
{
    m_currentTileCandidates.clear();

    QStringList revealLines;
    for (Player& player : m_state.players) {
        const QList<int> awarded = m_pendingTileAwards.value(player.id);
        QStringList chosenTexts;

        for (int tileId : awarded) {
            Tile* tile = m_state.tileById(tileId);
            if (!tile || tile->ownerId >= 0) {
                continue;
            }

            tile->ownerId = player.id;
            if (!player.ownedTileIds.contains(tileId)) {
                player.ownedTileIds.append(tileId);
            }
            chosenTexts << QString::number(tileId);
            emit tileUpdated(tileId);
        }

        emit playerUpdated(player.id);
        revealLines << QStringLiteral("%1：%2 个（%3）")
                           .arg(player.name)
                           .arg(chosenTexts.size())
                           .arg(chosenTexts.isEmpty() ? QStringLiteral("无") : chosenTexts.join(QStringLiteral("、")));
    }

    m_pendingTileAwards.clear();
    emit stateChanged();

    emit messageGenerated(QStringLiteral("所有玩家都已抽完地块，现在统一在地图上揭示：\n%1")
                          .arg(revealLines.join(QStringLiteral("\n"))));
    emit messageGenerated(QStringLiteral("抽地块阶段结束。稍后进入抽店铺牌阶段。"));
    QTimer::singleShot(kPhaseDelayMsLocal, this, [this] {
        autoDrawShopCardsForAllPlayers();

        QTimer::singleShot(kPhaseDelayMsLocal, this, [this] {
            for (Player& player : m_state.players) {
                player.tradeFinishedThisRound = false;
                player.builtShopThisRound = false;
            }
            m_state.phase = GamePhase::Trade;
            emit phaseChanged(m_state.phase);
            emit messageGenerated(QStringLiteral("进入交易阶段：点击左侧玩家头像发起交易；交易必须由对方选择同意后才成立，也可以选择讨价还价重新发条件。已点击结束交易的玩家不能再参与任何交易。单机模式下，非本机玩家会在本机玩家点击结束交易后自动结束；联网房间中四名玩家都需要由各自客户端手动结束。"));

            emit stateChanged();
        });
    });
}

void GameController::autoDrawShopCardsForAllPlayers()
{
    const BuildingDealRule rule = dealRuleFor(m_state.players.size(), m_state.round);
    emit messageGenerated(QStringLiteral("开始抽店铺牌：本轮每名玩家抽取 %1 张店铺牌（按表 1 保留数量）。剩余牌堆不足时自动少发。").arg(rule.keepCount));

    for (Player& player : m_state.players) {
        if (!player.active) {
            continue;
        }

        QStringList gained;
        for (int i = 0; i < rule.keepCount; ++i) {
            const ShopType type = randomShopTypeFromDeck();
            if (type == ShopType::None) {
                break;
            }
            m_state.shopDeckRemaining[type] = std::max(0, m_state.shopDeckRemaining.value(type, 0) - 1);
            player.shopCards[type] = player.shopCards.value(type, 0) + 1;
            gained << shopTypeName(type);
        }

        emit playerUpdated(player.id);
        emit messageGenerated(QStringLiteral("%1 获得店铺牌：%2。")
                              .arg(player.name)
                              .arg(gained.isEmpty() ? QStringLiteral("无，店铺牌堆已空") : gained.join(QStringLiteral("、"))));
    }

    emit stateChanged();
}

ShopType GameController::randomAvailableShopType(const Player& player) const
{
    QList<ShopType> available;
    for (ShopType type : allShopTypes()) {
        if (player.shopCards.value(type, 0) > 0) {
            available.append(type);
        }
    }

    if (available.isEmpty()) {
        return ShopType::None;
    }

    return available.at(QRandomGenerator::global()->bounded(int(available.size())));
}

ShopType GameController::randomShopTypeFromDeck() const
{
    int total = 0;
    for (ShopType type : allShopTypes()) {
        total += std::max(0, m_state.shopDeckRemaining.value(type, 0));
    }

    if (total <= 0) {
        return ShopType::None;
    }

    int pick = QRandomGenerator::global()->bounded(total);
    for (ShopType type : allShopTypes()) {
        const int count = std::max(0, m_state.shopDeckRemaining.value(type, 0));
        if (pick < count) {
            return type;
        }
        pick -= count;
    }

    return ShopType::None;
}

void GameController::beginBuildStage()
{
    m_state.phase = GamePhase::Build;
    m_buildPlayerIndex = 0;
    m_state.currentPlayerIndex = 0;

    emit phaseChanged(m_state.phase);
    emit messageGenerated(QStringLiteral("进入建设阶段：轮到的玩家可以批量建设，也可以直接跳过/完成。店铺必须建在自己拥有的空地块上。"));
    emit stateChanged();

    QTimer::singleShot(kPhaseDelayMsLocal, this, &GameController::requestBuildChoiceForNextPlayer);
}

bool GameController::playerHasBuildOptions(int playerId) const
{
    const Player* player = m_state.playerById(playerId);
    if (!player) {
        return false;
    }

    if (emptyOwnedTileIds(playerId).isEmpty()) {
        return false;
    }

    for (ShopType type : allShopTypes()) {
        if (player->shopCards.value(type, 0) > 0) {
            return true;
        }
    }
    return false;
}

void GameController::requestBuildChoiceForNextPlayer()
{
    if (m_state.phase != GamePhase::Build) {
        return;
    }

    while (m_buildPlayerIndex < m_state.players.size() &&
           !m_state.players[m_buildPlayerIndex].active) {
        ++m_buildPlayerIndex;
    }

    if (m_buildPlayerIndex >= m_state.players.size()) {
        beginIncomeStage();
        return;
    }

    m_state.currentPlayerIndex = m_buildPlayerIndex;
    const Player& player = m_state.players[m_buildPlayerIndex];
    if (!playerHasBuildOptions(player.id)) {
        emit messageGenerated(QStringLiteral("%1 当前没有可建设的空地块或店铺牌，跳过建设。").arg(player.name));
        ++m_buildPlayerIndex;
        QTimer::singleShot(kPhaseDelayMsLocal, this, &GameController::requestBuildChoiceForNextPlayer);
        return;
    }

    emit messageGenerated(QStringLiteral("轮到 %1 建设：请选择店铺牌并放到自己的空地块上。").arg(player.name));

    if (shouldAutoplayPlayer(player.id)) {
        const int autoPlayerId = player.id;
        emit messageGenerated(QStringLiteral("%1 为单机自动玩家，将自动建设一个可用店铺；没有可建选项则自动跳过。").arg(player.name));

        QTimer::singleShot(kPhaseDelayMsLocal, this, [this, autoPlayerId] {
            if (m_state.phase != GamePhase::Build) {
                return;
            }

            const Player* autoPlayer = m_state.playerById(autoPlayerId);
            if (!autoPlayer) {
                return;
            }

            bool built = false;
            for (ShopType type : allShopTypes()) {
                if (autoPlayer->shopCards.value(type, 0) <= 0) {
                    continue;
                }

                const QList<int> candidateTiles = emptyOwnedTileIds(autoPlayerId);
                for (int tileId : candidateTiles) {
                    QString errorMessage;
                    if (buildShop(autoPlayerId, type, QList<int>{tileId}, &errorMessage)) {
                        built = true;
                        break;
                    }
                }

                if (built) {
                    break;
                }
            }

            if (!built) {
                const Player* refreshedPlayer = m_state.playerById(autoPlayerId);
                emit messageGenerated(QStringLiteral("%1 没有可自动建设的店铺，跳过建设。")
                                      .arg(refreshedPlayer ? refreshedPlayer->name : QStringLiteral("单机自动玩家")));
            }

            finishBuildChoicesForPlayer(autoPlayerId);
        });
        return;
    }

    emit buildChoiceRequested(player.id);
}

void GameController::finishBuildChoicesForPlayer(int playerId)
{
    if (m_state.phase != GamePhase::Build) {
        return;
    }

    if (m_buildPlayerIndex >= 0 && m_buildPlayerIndex < m_state.players.size() &&
        m_state.players[m_buildPlayerIndex].id == playerId) {
        const Player* player = m_state.playerById(playerId);
        emit messageGenerated(QStringLiteral("%1 完成建设选择。")
                              .arg(player ? player->name : QStringLiteral("该玩家")));
        ++m_buildPlayerIndex;
        if (!m_state.players.isEmpty()) {
            m_state.currentPlayerIndex = std::min(m_buildPlayerIndex, int(m_state.players.size()) - 1);
        }
        emit stateChanged();
    }

    QTimer::singleShot(kPhaseDelayMsLocal, this, &GameController::requestBuildChoiceForNextPlayer);
}

void GameController::beginIncomeStage()
{
    QTimer::singleShot(kPhaseDelayMsLocal, this, [this] {
        m_state.phase = GamePhase::Income;
        emit phaseChanged(m_state.phase);
        emit messageGenerated(QStringLiteral("进入收入结算阶段：只根据图板上已建设店铺结算收入。"));
        emit stateChanged();

        QTimer::singleShot(kPhaseDelayMsLocal, this, [this] {
            applyIncomeForAllPlayers();
            beginRoundEndStage();
        });
    });
}

void GameController::beginRoundEndStage()
{
    QTimer::singleShot(kPhaseDelayMsLocal, this, [this] {
        m_state.phase = GamePhase::RoundEnd;
        emit phaseChanged(m_state.phase);
        emit messageGenerated(QStringLiteral("第 %1 轮结束。").arg(m_state.round));
        emit stateChanged();

        if (m_state.round >= kOfficialMaxRounds) {
            
            m_state.finalSettlementReady = true;
            emit messageGenerated(finalScoreMessage(m_state));
            m_isAdvancingRound = false;
            emit stateChanged();
            return;
        }

        ++m_state.round;
        QTimer::singleShot(kPhaseDelayMsLocal, this, &GameController::beginAutomaticRound);
    });
}

void GameController::markTradeFinishedForPlayer(int playerId)
{
    Player* player = m_state.playerById(playerId);
    if (!player || m_state.phase != GamePhase::Trade) {
        return;
    }

    if (!player->tradeFinishedThisRound) {
        player->tradeFinishedThisRound = true;
        emit playerUpdated(player->id);
        emit stateChanged();
    }
}

void GameController::finishTradeAndAdvanceRound()
{
    if (m_state.phase != GamePhase::Trade) {
        emit messageGenerated(QStringLiteral("当前不是交易阶段，不能结束交易。"));
        return;
    }

    if (m_isAdvancingRound) {
        emit messageGenerated(QStringLiteral("系统正在推进流程，请勿重复触发。"));
        return;
    }

    markTradeFinishedForPlayer(m_state.localPlayerId);

    if (m_autoplayNonLocalPlayers) {
        QList<int> autoPlayerIds;
        for (const Player& player : m_state.players) {
            if (player.active && player.id != m_state.localPlayerId && !player.tradeFinishedThisRound) {
                autoPlayerIds.append(player.id);
            }
        }
        for (int playerId : autoPlayerIds) {
            const Player* player = m_state.playerById(playerId);
            markTradeFinishedForPlayer(playerId);
            emit messageGenerated(QStringLiteral("%1 为单机自动玩家，已自动结束交易。")
                                  .arg(player ? player->name : QStringLiteral("单机自动玩家")));
        }
    }

    QStringList waiting;
    for (const Player& player : m_state.players) {
        if (player.active && !player.tradeFinishedThisRound) {
            waiting << player.name;
        }
    }

    if (!waiting.isEmpty()) {
        emit messageGenerated(QStringLiteral("已记录本玩家结束交易，仍在等待：%1。")
                              .arg(waiting.join(QStringLiteral("、"))));
        return;
    }

    m_isAdvancingRound = true;
    emit messageGenerated(QStringLiteral("所有玩家均已结束交易。稍后进入建设阶段。"));
    QTimer::singleShot(kPhaseDelayMsLocal, this, &GameController::beginBuildStage);
}

void GameController::nextPhase()
{
    if (m_state.phase == GamePhase::Trade) {
        finishTradeAndAdvanceRound();
        return;
    }

    if (m_state.phase == GamePhase::RoundEnd && m_state.round >= kOfficialMaxRounds) {
        emit messageGenerated(finalScoreMessage(m_state));
        return;
    }

    emit messageGenerated(QStringLiteral("游戏流程由系统按 6 轮官方顺序推进，请先完成当前阶段操作。"));
}

void GameController::nextTurn()
{
    if (m_state.players.isEmpty()) {
        return;
    }

    int nextId = m_state.localPlayerId + 1;
    if (nextId > m_state.players.size()) {
        nextId = 1;
    }
    setLocalPlayerId(nextId);
}

void GameController::setCurrentPlayer(int playerId)
{
    setLocalPlayerId(playerId);
}

void GameController::setLocalPlayerId(int playerId)
{
    for (int i = 0; i < m_state.players.size(); ++i) {
        if (m_state.players[i].id == playerId && m_state.players[i].active) {
            const int oldPlayerId = m_state.localPlayerId;
            m_state.localPlayerId = playerId;
            
            emit playerUpdated(oldPlayerId);
            emit playerUpdated(playerId);
            emit messageGenerated(QStringLiteral("本机玩家切换为：%1").arg(m_state.players[i].name));
            emit stateChanged();
            return;
        }
    }
}

void GameController::setPlayerDisplayName(int playerId, const QString& displayName)
{
    const QString cleaned = displayName.trimmed();
    if (cleaned.isEmpty()) {
        return;
    }

    Player* player = m_state.playerById(playerId);
    if (!player || player->name == cleaned) {
        return;
    }

    player->name = cleaned.left(16);
    emit playerUpdated(playerId);
    emit stateChanged();
}

void GameController::selectTile(int tileId)
{
    if (!m_state.tileById(tileId)) {
        return;
    }

    const int oldTile = m_state.selectedTileId;
    m_state.selectedTileId = tileId;

    if (oldTile > 0) {
        emit tileUpdated(oldTile);
    }
    emit tileUpdated(tileId);
    emit tileSelected(tileId);
    emit messageGenerated(QStringLiteral("选择地块 %1").arg(tileId));
    emit stateChanged();
}

void GameController::clearSelectedTile()
{
    const int oldTile = m_state.selectedTileId;
    m_state.selectedTileId = -1;

    if (oldTile > 0) {
        emit tileUpdated(oldTile);
    }

    emit tileSelected(-1);
    emit stateChanged();
}

QList<int> GameController::adjacentTiles(int tileId) const
{
    const Tile* tile = m_state.tileById(tileId);
    return tile ? tile->adjacentIds : QList<int>{};
}

QList<int> GameController::buildableTilesForPlayer(int playerId) const
{
    QList<int> result;
    for (const Tile& tile : m_state.tiles) {
        if (tile.ownerId == playerId && tile.shopType == ShopType::None && !tile.locked) {
            result.append(tile.id);
        }
    }
    return result;
}

QList<int> GameController::buildableTilesForCurrentPlayer() const
{
    return buildableTilesForPlayer(m_state.currentPlayerId());
}

void GameController::drawTileForCurrentPlayer()
{
    emit messageGenerated(QStringLiteral("地块由系统在每轮开始时发候选牌，并由玩家选择保留。"));
}

bool GameController::areTilesConnected(const QList<int>& tileIds) const
{
    if (tileIds.size() <= 1) {
        return true;
    }

    QSet<int> target;
    for (int id : tileIds) {
        target.insert(id);
    }

    QSet<int> visited;
    QQueue<int> queue;

    queue.enqueue(tileIds.first());
    visited.insert(tileIds.first());

    while (!queue.isEmpty()) {
        const int current = queue.dequeue();
        const Tile* tile = m_state.tileById(current);
        if (!tile) {
            continue;
        }

        for (int next : tile->adjacentIds) {
            if (target.contains(next) && !visited.contains(next)) {
                visited.insert(next);
                queue.enqueue(next);
            }
        }
    }

    return visited.size() == target.size();
}

bool GameController::canBuildShop(int playerId,
                                  ShopType type,
                                  const QList<int>& tileIds,
                                  QString* reason) const
{
    QList<int> effectiveTileIds = tileIds;
    if (effectiveTileIds.isEmpty() && m_state.selectedTileId > 0) {
        effectiveTileIds.append(m_state.selectedTileId);
    }

    const Player* player = m_state.playerById(playerId);
    if (!player) {
        if (reason) *reason = QStringLiteral("玩家不存在");
        return false;
    }

    if (m_state.phase != GamePhase::Build) {
        if (reason) *reason = QStringLiteral("只有建设阶段可以建设店铺");
        return false;
    }

    if (m_state.currentPlayerId() != playerId) {
        const Player* current = m_state.currentPlayer();
        if (reason) *reason = QStringLiteral("当前轮到 %1 建设，请不要越过当前玩家。")
                .arg(current ? current->name : QStringLiteral("其他玩家"));
        return false;
    }

    if (type == ShopType::None) {
        if (reason) *reason = QStringLiteral("请选择店铺类型");
        return false;
    }

    if (effectiveTileIds.size() != 1) {
        if (reason) *reason = QStringLiteral("一张店铺牌必须且只能建到 1 个空地块上");
        return false;
    }

    if (player->shopCards.value(type, 0) <= 0) {
        if (reason) *reason = QStringLiteral("没有可用的%1牌").arg(shopTypeName(type));
        return false;
    }

    const int tileId = effectiveTileIds.first();
    const Tile* tile = m_state.tileById(tileId);
    if (!tile) {
        if (reason) *reason = QStringLiteral("地块 %1 不存在").arg(tileId);
        return false;
    }

    if (tile->ownerId != playerId) {
        if (reason) *reason = QStringLiteral("地块 %1 不属于该玩家，不能在别人地块上建设").arg(tileId);
        return false;
    }

    if (tile->locked) {
        if (reason) *reason = QStringLiteral("地块 %1 已被锁定").arg(tileId);
        return false;
    }

    if (tile->shopType != ShopType::None) {
        if (reason) *reason = QStringLiteral("地块 %1 已经建设了店铺").arg(tileId);
        return false;
    }

    if (reason) *reason = QStringLiteral("可以建设");
    return true;
}

bool GameController::buildShop(int playerId,
                               ShopType type,
                               const QList<int>& tileIds,
                               QString* errorMessage)
{
    QList<int> effectiveTileIds = tileIds;
    if (effectiveTileIds.isEmpty() && m_state.selectedTileId > 0) {
        effectiveTileIds.append(m_state.selectedTileId);
    }

    QString reason;
    if (!canBuildShop(playerId, type, effectiveTileIds, &reason)) {
        if (errorMessage) {
            *errorMessage = reason;
        }
        emit messageGenerated(reason);
        return false;
    }

    Player* player = m_state.playerById(playerId);
    if (!player) {
        return false;
    }

    Shop shop;
    shop.id = m_state.nextShopId++;
    shop.ownerId = playerId;
    shop.type = type;
    shop.tileIds = effectiveTileIds;

    player->shopCards[type] = player->shopCards.value(type, 0) - 1;
    m_state.shops.append(shop);
    normalizeShopOwnership();

    for (int tileId : std::as_const(effectiveTileIds)) {
        emit tileUpdated(tileId);
    }

    emit playerUpdated(playerId);
    emit stateChanged();

    emit messageGenerated(QStringLiteral("%1 在地块 %2 建设了%3。")
                          .arg(player->name)
                          .arg(effectiveTileIds.first())
                          .arg(shopTypeName(type)));

    return true;
}

static void countAsset(QMap<int, int>& counter, int key)
{
    counter[key] = counter.value(key, 0) + 1;
}

bool GameController::validateTrade(const TradeProposal& proposal,
                                   QString* reason) const
{
    const Player* left = m_state.playerById(proposal.leftPlayerId);
    const Player* right = m_state.playerById(proposal.rightPlayerId);

    if (!left || !right) {
        if (reason) *reason = QStringLiteral("交易双方不存在");
        return false;
    }

    if (left->id == right->id) {
        if (reason) *reason = QStringLiteral("不能和自己交易");
        return false;
    }

    if (m_state.phase != GamePhase::Trade) {
        if (reason) *reason = QStringLiteral("只有交易阶段可以交易");
        return false;
    }

    if (left->tradeFinishedThisRound || right->tradeFinishedThisRound) {
        const Player* ended = left->tradeFinishedThisRound ? left : right;
        if (reason) *reason = QStringLiteral("%1 已经结束交易，不能再参与任何交易").arg(ended->name);
        return false;
    }

    if (proposal.leftAssets.isEmpty() &&
        proposal.rightAssets.isEmpty() &&
        proposal.leftMoney == 0 &&
        proposal.rightMoney == 0) {
        if (reason) *reason = QStringLiteral("交易内容不能为空");
        return false;
    }

    if (proposal.leftMoney < 0 || proposal.rightMoney < 0) {
        if (reason) *reason = QStringLiteral("交易金额不能为负数");
        return false;
    }

    if (proposal.leftMoney > 1000000 || proposal.rightMoney > 1000000) {
        if (reason) *reason = QStringLiteral("单方交易金额不能超过 1,000,000");
        return false;
    }

    auto validateAssetsFor = [&](const Player* player, const QList<TradeAsset>& assets, QString* error) -> bool {
        QMap<int, int> offeredShopCards;
        QSet<int> offeredTiles;

        for (const TradeAsset& asset : assets) {
            if (asset.type == TradeAssetType::Tile) {
                if (offeredTiles.contains(asset.id)) {
                    if (error) *error = QStringLiteral("地块 %1 被重复放入交易").arg(asset.id);
                    return false;
                }
                offeredTiles.insert(asset.id);

                const Tile* tile = m_state.tileById(asset.id);
                if (!tile || tile->ownerId != player->id) {
                    if (error) *error = QStringLiteral("%1 不拥有地块 %2").arg(player->name).arg(asset.id);
                    return false;
                }
                if (tile->locked) {
                    if (error) *error = QStringLiteral("地块 %1 已被锁定").arg(asset.id);
                    return false;
                }
                if (tile->shopType != ShopType::None && tile->shopOwnerId > 0 && tile->shopOwnerId != player->id) {
                    if (error) *error = QStringLiteral("地块 %1 上的店铺不属于 %2，不能把店铺和地块拆开交易").arg(asset.id).arg(player->name);
                    return false;
                }
            } else if (asset.type == TradeAssetType::ShopCard) {
                if (asset.shopType == ShopType::None) {
                    if (error) *error = QStringLiteral("交易中的店铺牌类型无效");
                    return false;
                }
                const int key = static_cast<int>(asset.shopType);
                countAsset(offeredShopCards, key);
                if (offeredShopCards.value(key) > player->shopCards.value(asset.shopType, 0)) {
                    if (error) *error = QStringLiteral("%1 没有足够的%2牌")
                            .arg(player->name)
                            .arg(shopTypeName(asset.shopType));
                    return false;
                }
            } else if (asset.type == TradeAssetType::Shop) {
                if (error) *error = QStringLiteral("已建店铺不能脱离地块单独交易；请选择承载该店铺的地块，系统会把地块和上面的店铺打包转让");
                return false;
            }
        }

        return true;
    };

    QString error;
    if (!validateAssetsFor(left, proposal.leftAssets, &error)) {
        if (reason) *reason = error;
        return false;
    }

    if (!validateAssetsFor(right, proposal.rightAssets, &error)) {
        if (reason) *reason = error;
        return false;
    }

    if (reason) *reason = QStringLiteral("交易合法");
    return true;
}

void GameController::transferAsset(int fromPlayerId, int toPlayerId, const TradeAsset& asset)
{
    Player* from = m_state.playerById(fromPlayerId);
    Player* to = m_state.playerById(toPlayerId);
    if (!from || !to) {
        return;
    }

    auto transferShopOwnership = [&](int shopId) {
        Shop* globalShop = m_state.shopById(shopId);
        if (!globalShop || globalShop->ownerId != fromPlayerId) {
            return;
        }

        globalShop->ownerId = toPlayerId;

        for (int i = from->shops.size() - 1; i >= 0; --i) {
            if (from->shops[i].id == shopId) {
                from->shops.removeAt(i);
            }
        }

        Shop movedShop = *globalShop;
        for (int i = to->shops.size() - 1; i >= 0; --i) {
            if (to->shops[i].id == shopId) {
                to->shops.removeAt(i);
            }
        }
        to->shops.append(movedShop);

        for (int tileId : std::as_const(globalShop->tileIds)) {
            Tile* tile = m_state.tileById(tileId);
            if (tile && tile->shopOwnerId == fromPlayerId) {
                tile->shopOwnerId = toPlayerId;
                emit tileUpdated(tileId);
            }
        }
    };

    if (asset.type == TradeAssetType::Tile) {
        Tile* tile = m_state.tileById(asset.id);
        if (!tile) {
            return;
        }

        from->ownedTileIds.removeAll(asset.id);
        if (!to->ownedTileIds.contains(asset.id)) {
            to->ownedTileIds.append(asset.id);
        }

        tile->ownerId = toPlayerId;

        
        for (Shop& shop : m_state.shops) {
            if (shopOccupiesTile(shop, asset.id)) {
                shop.ownerId = toPlayerId;
                for (int shopTileId : std::as_const(shop.tileIds)) {
                    Tile* shopTile = m_state.tileById(shopTileId);
                    if (shopTile) {
                        shopTile->ownerId = toPlayerId;
                        from->ownedTileIds.removeAll(shopTileId);
                        if (!to->ownedTileIds.contains(shopTileId)) {
                            to->ownedTileIds.append(shopTileId);
                        }
                        emit tileUpdated(shopTileId);
                    }
                }
            }
        }

        emit tileUpdated(asset.id);
    } else if (asset.type == TradeAssetType::ShopCard) {
        from->shopCards[asset.shopType] = from->shopCards.value(asset.shopType, 0) - 1;
        to->shopCards[asset.shopType] = to->shopCards.value(asset.shopType, 0) + 1;
    } else if (asset.type == TradeAssetType::Shop) {
        transferShopOwnership(asset.id);
    }
}

void GameController::normalizeShopOwnership()
{
    
    
    
    for (Player& player : m_state.players) {
        player.shops.clear();
    }

    QSet<int> seenShopIds;
    for (int i = m_state.shops.size() - 1; i >= 0; --i) {
        Shop& shop = m_state.shops[i];
        if (shop.id <= 0 || shop.type == ShopType::None || seenShopIds.contains(shop.id) || shop.tileIds.isEmpty()) {
            m_state.shops.removeAt(i);
            continue;
        }
        seenShopIds.insert(shop.id);

        const Tile* anchorTile = m_state.tileById(shop.tileIds.first());
        if (!anchorTile || anchorTile->ownerId <= 0) {
            m_state.shops.removeAt(i);
            continue;
        }
        shop.ownerId = anchorTile->ownerId;
    }

    for (Tile& tile : m_state.tiles) {
        tile.shopType = ShopType::None;
        tile.shopOwnerId = -1;
    }

    for (Shop& shop : m_state.shops) {
        Player* owner = m_state.playerById(shop.ownerId);
        if (!owner) {
            continue;
        }

        owner->shops.append(shop);

        for (int tileId : std::as_const(shop.tileIds)) {
            Tile* tile = m_state.tileById(tileId);
            if (!tile) {
                continue;
            }

            tile->ownerId = shop.ownerId;
            tile->shopType = shop.type;
            tile->shopOwnerId = shop.ownerId;
        }
    }

    
    for (Player& player : m_state.players) {
        player.ownedTileIds.clear();
    }
    for (const Tile& tile : m_state.tiles) {
        if (Player* owner = m_state.playerById(tile.ownerId)) {
            if (!owner->ownedTileIds.contains(tile.id)) {
                owner->ownedTileIds.append(tile.id);
            }
        }
    }
}

bool GameController::applyTrade(const TradeProposal& proposal,
                                QString* errorMessage)
{
    QString reason;
    if (!validateTrade(proposal, &reason)) {
        if (errorMessage) {
            *errorMessage = reason;
        }
        emit messageGenerated(reason);
        return false;
    }

    Player* left = m_state.playerById(proposal.leftPlayerId);
    Player* right = m_state.playerById(proposal.rightPlayerId);
    if (!left || !right) {
        return false;
    }

    left->money -= proposal.leftMoney;
    right->money += proposal.leftMoney;

    right->money -= proposal.rightMoney;
    left->money += proposal.rightMoney;

    for (const TradeAsset& asset : proposal.leftAssets) {
        transferAsset(left->id, right->id, asset);
    }

    for (const TradeAsset& asset : proposal.rightAssets) {
        transferAsset(right->id, left->id, asset);
    }

    normalizeShopOwnership();

    emit playerUpdated(left->id);
    emit playerUpdated(right->id);
    emit stateChanged();
    emit messageGenerated(QStringLiteral("%1 与 %2 完成交易").arg(left->name, right->name));

    return true;
}

QJsonObject GameController::exportStateToJson() const
{
    QJsonObject object;
    object[QStringLiteral("version")] = 3;
    object[QStringLiteral("round")] = m_state.round;
    object[QStringLiteral("phase")] = static_cast<int>(m_state.phase);
    object[QStringLiteral("currentPlayerIndex")] = m_state.currentPlayerIndex;
    object[QStringLiteral("localPlayerId")] = m_state.localPlayerId;
    object[QStringLiteral("selectedTileId")] = m_state.selectedTileId;
    object[QStringLiteral("nextShopId")] = m_state.nextShopId;
    object[QStringLiteral("finalSettlementReady")] = m_state.finalSettlementReady;
    object[QStringLiteral("boardThemeKey")] = m_state.boardThemeKey;
    object[QStringLiteral("tileChoicePlayerIndex")] = m_tileChoicePlayerIndex;
    object[QStringLiteral("buildPlayerIndex")] = m_buildPlayerIndex;
    object[QStringLiteral("currentTileCandidates")] = intListToJsonArray(m_currentTileCandidates);
    QJsonObject pendingAwardsObject;
    for (auto it = m_pendingTileAwards.constBegin(); it != m_pendingTileAwards.constEnd(); ++it) {
        pendingAwardsObject[QString::number(it.key())] = intListToJsonArray(it.value());
    }
    object[QStringLiteral("pendingTileAwards")] = pendingAwardsObject;
    object[QStringLiteral("isAdvancingRound")] = m_isAdvancingRound;

    QJsonArray players;
    for (const Player& player : m_state.players) {
        QJsonObject p;
        p[QStringLiteral("id")] = player.id;
        p[QStringLiteral("name")] = player.name;
        p[QStringLiteral("color")] = player.color.name(QColor::HexArgb);
        p[QStringLiteral("money")] = player.money;
        p[QStringLiteral("ownedTileIds")] = intListToJsonArray(player.ownedTileIds);
        p[QStringLiteral("shopCards")] = shopCardsToJsonObject(player.shopCards);
        p[QStringLiteral("active")] = player.active;
        p[QStringLiteral("builtShopThisRound")] = player.builtShopThisRound;
        p[QStringLiteral("tradeFinishedThisRound")] = player.tradeFinishedThisRound;
        players.append(p);
    }
    object[QStringLiteral("players")] = players;

    QJsonArray tiles;
    for (const Tile& tile : m_state.tiles) {
        tiles.append(tileToJson(tile));
    }
    object[QStringLiteral("tiles")] = tiles;

    QJsonArray shops;
    for (const Shop& shop : m_state.shops) {
        shops.append(shopToJson(shop));
    }
    object[QStringLiteral("shops")] = shops;

    object[QStringLiteral("shopDeckRemaining")] = shopCardsToJsonObject(m_state.shopDeckRemaining);
    return object;
}

void GameController::importStateFromJson(const QJsonObject& object, bool keepLocalPlayer)
{
    if (object.isEmpty()) {
        return;
    }

    const int previousLocalPlayerId = m_state.localPlayerId;

    GameState imported;
    imported.round = object.value(QStringLiteral("round")).toInt(1);
    imported.phase = static_cast<GamePhase>(object.value(QStringLiteral("phase")).toInt(static_cast<int>(GamePhase::DrawTile)));
    imported.currentPlayerIndex = object.value(QStringLiteral("currentPlayerIndex")).toInt(0);
    imported.localPlayerId = keepLocalPlayer
        ? previousLocalPlayerId
        : object.value(QStringLiteral("localPlayerId")).toInt(1);
    imported.selectedTileId = object.value(QStringLiteral("selectedTileId")).toInt(-1);
    imported.nextShopId = object.value(QStringLiteral("nextShopId")).toInt(1);
    imported.finalSettlementReady = object.value(QStringLiteral("finalSettlementReady")).toBool(false);
    imported.boardThemeKey = object.value(QStringLiteral("boardThemeKey")).toString(QStringLiteral("porcelain"));
    if (imported.boardThemeKey.trimmed().isEmpty()) {
        imported.boardThemeKey = QStringLiteral("porcelain");
    }

    const QJsonArray playersArray = object.value(QStringLiteral("players")).toArray();
    for (const QJsonValue& value : playersArray) {
        const QJsonObject p = value.toObject();
        Player player;
        player.id = p.value(QStringLiteral("id")).toInt(-1);
        player.name = p.value(QStringLiteral("name")).toString(QStringLiteral("玩家%1").arg(player.id));
        player.color = QColor(p.value(QStringLiteral("color")).toString());
        player.money = p.value(QStringLiteral("money")).toInt(0);
        player.ownedTileIds = intListFromJsonArray(p.value(QStringLiteral("ownedTileIds")).toArray());
        player.shopCards = shopCardsFromJsonObject(p.value(QStringLiteral("shopCards")).toObject());
        player.active = p.value(QStringLiteral("active")).toBool(true);
        player.builtShopThisRound = p.value(QStringLiteral("builtShopThisRound")).toBool(false);
        player.tradeFinishedThisRound = p.value(QStringLiteral("tradeFinishedThisRound")).toBool(false);
        if (player.id > 0) {
            imported.players.append(player);
        }
    }

    
    normalizePlayerColors(imported.players);

    if (imported.players.isEmpty()) {
        imported.currentPlayerIndex = 0;
    } else {
        imported.currentPlayerIndex = std::clamp(imported.currentPlayerIndex, 0, int(imported.players.size()) - 1);
    }

    const QJsonArray tilesArray = object.value(QStringLiteral("tiles")).toArray();
    for (const QJsonValue& value : tilesArray) {
        Tile tile = tileFromJson(value.toObject());
        if (tile.id > 0) {
            imported.tiles.append(tile);
        }
    }

    const QJsonArray shopsArray = object.value(QStringLiteral("shops")).toArray();
    for (const QJsonValue& value : shopsArray) {
        Shop shop = shopFromJson(value.toObject());
        if (shop.id > 0 && shop.type != ShopType::None) {
            imported.shops.append(shop);
            imported.nextShopId = std::max(imported.nextShopId, shop.id + 1);
        }
    }

    
    if (imported.shops.isEmpty()) {
        for (const Tile& tile : imported.tiles) {
            if (tile.shopType != ShopType::None && tile.shopOwnerId > 0) {
                Shop shop;
                shop.id = imported.nextShopId++;
                shop.ownerId = tile.shopOwnerId;
                shop.type = tile.shopType;
                shop.tileIds = QList<int>{tile.id};
                imported.shops.append(shop);
            }
        }
    }

    const QJsonObject deckObject = object.value(QStringLiteral("shopDeckRemaining")).toObject();
    if (deckObject.isEmpty()) {
        imported.shopDeckRemaining = reconstructShopDeck(imported.players, imported.shops);
    } else {
        imported.shopDeckRemaining = shopCardsFromJsonObject(deckObject);
    }

    
    for (Player& player : imported.players) {
        player.ownedTileIds.clear();
        if (player.shopCards.isEmpty()) {
            for (ShopType type : allShopTypes()) {
                player.shopCards[type] = 0;
            }
        }
    }
    for (const Tile& tile : imported.tiles) {
        if (Player* owner = imported.playerById(tile.ownerId)) {
            if (!owner->ownedTileIds.contains(tile.id)) {
                owner->ownedTileIds.append(tile.id);
            }
        }
    }

    const int maxPlayerIndex = std::max(0, int(imported.players.size()));
    m_tileChoicePlayerIndex = std::clamp(object.value(QStringLiteral("tileChoicePlayerIndex")).toInt(imported.currentPlayerIndex),
                                         0,
                                         maxPlayerIndex);
    m_buildPlayerIndex = std::clamp(object.value(QStringLiteral("buildPlayerIndex")).toInt(imported.currentPlayerIndex),
                                    0,
                                    maxPlayerIndex);
    m_currentTileCandidates = intListFromJsonArray(object.value(QStringLiteral("currentTileCandidates")).toArray());
    m_pendingTileAwards.clear();
    const QJsonObject pendingAwardsObject = object.value(QStringLiteral("pendingTileAwards")).toObject();
    for (auto it = pendingAwardsObject.constBegin(); it != pendingAwardsObject.constEnd(); ++it) {
        bool ok = false;
        const int playerId = it.key().toInt(&ok);
        if (ok) {
            m_pendingTileAwards[playerId] = intListFromJsonArray(it.value().toArray());
        }
    }
    m_isAdvancingRound = object.value(QStringLiteral("isAdvancingRound")).toBool(false);

    m_state = imported;
    m_selectedBoardThemeKey = m_state.boardThemeKey;
    normalizeShopOwnership();

    emit phaseChanged(m_state.phase);
    for (const Tile& tile : m_state.tiles) {
        emit tileUpdated(tile.id);
    }
    for (const Player& player : m_state.players) {
        emit playerUpdated(player.id);
    }
    emit tileSelected(m_state.selectedTileId);
    emit stateChanged();
}

int GameController::baseIncomeForShop(ShopType type) const
{
    return type == ShopType::None ? 0 : incompleteIncomeForSize(1);
}

int GameController::calculatePlayerIncome(int playerId) const
{
    if (!m_state.playerById(playerId)) {
        return 0;
    }

    int income = 0;
    QSet<int> visited;

    for (const Tile& startTile : m_state.tiles) {
        if (visited.contains(startTile.id) ||
            startTile.shopOwnerId != playerId ||
            startTile.shopType == ShopType::None) {
            continue;
        }

        const ShopType type = startTile.shopType;
        int groupSize = 0;
        QQueue<int> queue;
        queue.enqueue(startTile.id);
        visited.insert(startTile.id);

        while (!queue.isEmpty()) {
            const int currentId = queue.dequeue();
            ++groupSize;

            const Tile* currentTile = m_state.tileById(currentId);
            if (!currentTile) {
                continue;
            }

            for (int nextId : currentTile->adjacentIds) {
                if (visited.contains(nextId)) {
                    continue;
                }

                const Tile* nextTile = m_state.tileById(nextId);
                if (!nextTile) {
                    continue;
                }

                if (nextTile->shopOwnerId == playerId && nextTile->shopType == type) {
                    visited.insert(nextId);
                    queue.enqueue(nextId);
                }
            }
        }

        income += incomeForConnectedGroup(groupSize, maxSizeForShop(type));
    }

    return income;
}

void GameController::applyIncomeForAllPlayers()
{
    for (Player& player : m_state.players) {
        const int income = calculatePlayerIncome(player.id);
        player.money += income;
        emit playerUpdated(player.id);

        emit messageGenerated(QStringLiteral("%1 收入结算：+$%2，当前资金：$%3。")
                              .arg(player.name)
                              .arg(income)
                              .arg(player.money));
    }

    emit stateChanged();
}
