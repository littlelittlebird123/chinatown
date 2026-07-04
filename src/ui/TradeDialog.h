#pragma once

#include <QDialog>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>

#include "core/GameTypes.h"

class GameController;
class BoardView;

class TradeDialog : public QDialog {
    Q_OBJECT

public:
    explicit TradeDialog(QWidget* parent = nullptr);

    void setController(GameController* controller);
    void setPlayers(int leftPlayerId, int rightPlayerId);
    void setProposal(const TradeProposal& proposal);
    TradeProposal proposal() const { return m_proposal; }

    void refreshCards();
    void addAssetToOffer(bool leftSide, const TradeAsset& asset);
    void removeAssetFromOffer(bool leftSide, const TradeAsset& asset);
    void adjustMoney(bool leftSide, int delta);
    void pushHistory();
    void undo();
    void clearOffer();
    void confirmTrade();

    void setLeftReady(bool ready);
    void setRightReady(bool ready);
    void markProposalDirty();
    void lockOfferForPlayer(int playerId);
    void unlockOfferForPlayer(int playerId);

signals:
    void tradeConfirmed(const TradeProposal& proposal);

private:
    enum ItemRoles {
        AssetTypeRole = Qt::UserRole + 1,
        AssetIdRole,
        ShopTypeRole
    };

    QList<TradeAsset> assetsForPlayer(int playerId) const;
    QList<TradeAsset> filteredAssets(const QList<TradeAsset>& source,
                                     const QList<TradeAsset>& offered) const;

    QListWidgetItem* createAssetItem(const TradeAsset& asset) const;
    TradeAsset itemAsset(QListWidgetItem* item) const;

    void refreshSide(bool leftSide);
    void updateMoneyFromWidgets();
    void updateValidation();
    void refreshSummary();
    void setSideLocked(bool leftSide, bool locked);
    void openMapTileSelector();
    void updateTradeMapEffects(BoardView* view) const;
    bool tileOfferedBySide(bool leftSide, int tileId) const;
    bool toggleTileOfferFromMap(int tileId);

    GameController* m_controller = nullptr;
    TradeProposal m_proposal;
    QList<TradeProposal> m_history;

    QLabel* m_leftTitle = nullptr;
    QLabel* m_rightTitle = nullptr;
    QListWidget* m_leftAssets = nullptr;
    QListWidget* m_rightAssets = nullptr;
    QListWidget* m_leftOffer = nullptr;
    QListWidget* m_rightOffer = nullptr;
    QSpinBox* m_leftMoney = nullptr;
    QSpinBox* m_rightMoney = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QPushButton* m_confirmButton = nullptr;
};
