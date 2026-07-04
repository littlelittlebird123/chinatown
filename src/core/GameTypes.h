#pragma once

#include <QColor>
#include <QList>
#include <QMap>
#include <QPoint>
#include <QString>
#include <QStringList>


enum class ShopType {
    None = 0,
    Photo,
    Teahouse,
    Seafood,
    Jewelry,
    TropicalFish,
    Florist,
    TakeOut,
    Laundry,
    DimSum,
    Antiques,
    Factory,
    Restaurant,
    Market,
    Pharmacy
};

enum class GamePhase {
    DrawTile = 0,
    Trade,
    Build,
    Income,
    RoundEnd
};

enum class AssetFilter {
    All = 0,
    TileOnly,
    ShopOnly,
    BuildableOnly,
    TradeableOnly
};

enum class AssetSortMode {
    ById = 0,
    ByType,
    ByValue,
    ByShopType
};

enum class TradeAssetType {
    Tile = 0,
    ShopCard,
    Shop
};

struct TradeAsset {
    TradeAssetType type = TradeAssetType::Tile;
    int id = -1;
    ShopType shopType = ShopType::None;
};

inline bool operator==(const TradeAsset& a, const TradeAsset& b)
{
    return a.type == b.type && a.id == b.id && a.shopType == b.shopType;
}

struct Shop {
    int id = -1;
    int ownerId = -1;
    ShopType type = ShopType::None;
    
    
    QList<int> tileIds;
};

struct Tile {
    int id = -1;
    QPoint gridPos;
    int ownerId = -1;                
    ShopType shopType = ShopType::None;
    int shopOwnerId = -1;            
    QList<int> adjacentIds;
    bool locked = false;
    int baseValue = 1000;
    int district = 0;                
};

struct Player {
    int id = -1;
    QString name;
    QColor color;
    int money = 0; 
    QList<int> ownedTileIds;
    QMap<ShopType, int> shopCards;
    QList<Shop> shops;
    bool active = true;
    bool builtShopThisRound = false; 
    bool tradeFinishedThisRound = false; 
};

struct TradeProposal {
    int leftPlayerId = -1;
    int rightPlayerId = -1;
    QList<TradeAsset> leftAssets;
    QList<TradeAsset> rightAssets;
    int leftMoney = 0;
    int rightMoney = 0;
    bool leftReady = false;
    bool rightReady = false;
};

inline QList<ShopType> allShopTypes()
{
    return {
        ShopType::Photo,
        ShopType::Teahouse,
        ShopType::Seafood,
        ShopType::Jewelry,
        ShopType::TropicalFish,
        ShopType::Florist,
        ShopType::TakeOut,
        ShopType::Laundry,
        ShopType::DimSum,
        ShopType::Antiques,
        ShopType::Factory,
        ShopType::Restaurant
    };
}

inline QString shopTypeName(ShopType type)
{
    switch (type) {
    case ShopType::Photo:        return QStringLiteral("照相馆");
    case ShopType::Teahouse:     return QStringLiteral("茶馆");
    case ShopType::Seafood:      return QStringLiteral("海鲜店");
    case ShopType::Jewelry:      return QStringLiteral("珠宝店");
    case ShopType::TropicalFish: return QStringLiteral("观赏鱼店");
    case ShopType::Florist:      return QStringLiteral("鲜花店");
    case ShopType::TakeOut:      return QStringLiteral("外卖店");
    case ShopType::Laundry:      return QStringLiteral("洗衣店");
    case ShopType::DimSum:       return QStringLiteral("小吃店");
    case ShopType::Antiques:     return QStringLiteral("古董店");
    case ShopType::Factory:      return QStringLiteral("裁缝店");
    case ShopType::Restaurant:   return QStringLiteral("餐馆");
    case ShopType::Market:       return QStringLiteral("市场");
    case ShopType::Pharmacy:     return QStringLiteral("药店");
    case ShopType::None:         return QStringLiteral("无");
    }
    return QStringLiteral("未知");
}

inline QString shopTypeKey(ShopType type)
{
    switch (type) {
    case ShopType::Photo:        return QStringLiteral("photo");
    case ShopType::Teahouse:     return QStringLiteral("teahouse");
    case ShopType::Seafood:      return QStringLiteral("seafood");
    case ShopType::Jewelry:      return QStringLiteral("jewelry");
    case ShopType::TropicalFish: return QStringLiteral("tropical_fish");
    case ShopType::Florist:      return QStringLiteral("florist");
    case ShopType::TakeOut:      return QStringLiteral("take_out");
    case ShopType::Laundry:      return QStringLiteral("laundry");
    case ShopType::DimSum:       return QStringLiteral("dim_sum");
    case ShopType::Antiques:     return QStringLiteral("antiques");
    case ShopType::Factory:      return QStringLiteral("factory");
    case ShopType::Restaurant:   return QStringLiteral("restaurant");
    case ShopType::Market:       return QStringLiteral("market");
    case ShopType::Pharmacy:     return QStringLiteral("pharmacy");
    case ShopType::None:         return QStringLiteral("none");
    }
    return QStringLiteral("unknown");
}

inline int shopMaxSize(ShopType type)
{
    switch (type) {
    case ShopType::Photo:
    case ShopType::Teahouse:
    case ShopType::Seafood:
        return 3;
    case ShopType::Jewelry:
    case ShopType::TropicalFish:
    case ShopType::Florist:
    case ShopType::Pharmacy:
        return 4;
    case ShopType::TakeOut:
    case ShopType::Laundry:
    case ShopType::DimSum:
    case ShopType::Market:
        return 5;
    case ShopType::Antiques:
    case ShopType::Factory:
    case ShopType::Restaurant:
        return 6;
    case ShopType::None:
        return 0;
    }
    return 0;
}

inline int shopCardDeckCount(ShopType type)
{
    const int maxSize = shopMaxSize(type);
    return maxSize > 0 ? maxSize + 3 : 0; 
}

inline QString phaseName(GamePhase phase)
{
    switch (phase) {
    case GamePhase::DrawTile: return QStringLiteral("获得建筑卡牌阶段");
    case GamePhase::Trade:    return QStringLiteral("谈判交易阶段");
    case GamePhase::Build:    return QStringLiteral("放置店铺卡片阶段");
    case GamePhase::Income:   return QStringLiteral("获得收入阶段");
    case GamePhase::RoundEnd: return QStringLiteral("移动时间标志阶段");
    }
    return QStringLiteral("未知阶段");
}

inline QString tradeAssetName(const TradeAsset& asset)
{
    if (asset.type == TradeAssetType::Tile) {
        return QStringLiteral("建筑 %1").arg(asset.id);
    }
    if (asset.type == TradeAssetType::ShopCard) {
        return QStringLiteral("%1卡片").arg(shopTypeName(asset.shopType));
    }
    if (asset.type == TradeAssetType::Shop) {
        return QStringLiteral("%1店铺 #%2").arg(shopTypeName(asset.shopType)).arg(asset.id);
    }
    return QStringLiteral("未知资产");
}
