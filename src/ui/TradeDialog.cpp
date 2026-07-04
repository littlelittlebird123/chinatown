#include "ui/TradeDialog.h"

#include "core/GameController.h"
#include "ui/BoardView.h"
#include "ui/ToastMessage.h"
#include "utils/AnimationManager.h"

#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <algorithm>

TradeDialog::TradeDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("交易协商"));
    resize(940, 620);
    setMinimumSize(860, 560);
    setStyleSheet(QStringLiteral(
        "QDialog { background: #2b2118; }"
        "QLabel { color: #f9e6b7; }"
        "QLabel#tradeHeader { font-size: 20px; font-weight: 900; color: #ffe3a4; }"
        "QLabel#sideTitle { background: rgba(255, 226, 160, 28); border: 3px solid #c79556; border-radius: 14px; padding: 7px; font-size: 14px; font-weight: 800; }"
        "QLabel#summaryLabel { background: rgba(255, 244, 220, 236); color: #3a2a1d; border: 3px solid #be8f55; border-radius: 14px; padding: 8px; font-size: 13px; }"
        "QGroupBox { color: #ffe6b3; font-size: 14px; font-weight: 800; border: 3px solid #aa7c46; border-radius: 14px; margin-top: 9px; padding: 7px; background: rgba(20, 12, 7, 42); }"
        "QGroupBox::title { subcontrol-origin: margin; left: 14px; padding: 0 6px; }"
        "QListWidget { background: rgba(255, 243, 216, 242); color: #3b2b1e; border: 2px solid #b98a55; border-radius: 10px; padding: 5px; font-size: 13px; }"
        "QListWidget::item { padding: 5px 5px; border-bottom: 1px solid rgba(122, 91, 54, 60); }"
        "QListWidget::item:hover { background: rgba(210, 158, 82, 60); }"
        "QSpinBox { background: #fff1d0; color: #332318; border: 2px solid #b98a55; border-radius: 8px; padding: 6px; font-size: 15px; font-weight: 800; }"
        "QPushButton { background: #6b3f2b; color: #fff1c4; border: 2px solid #bd9258; border-radius: 9px; padding: 7px 12px; font-weight: 800; }"
        "QPushButton:hover { background: #845036; border-color: #f1d08c; }"
        "QPushButton:disabled { background: #46372d; color: #9c8d72; border-color: #6a5847; }"
    ));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 10, 12, 10);
    root->setSpacing(7);

    auto* header = new QLabel(QStringLiteral("交易协商：点击资产加入本次交易，点击已给出资产可撤回"), this);
    header->setObjectName(QStringLiteral("tradeHeader"));
    header->setAlignment(Qt::AlignCenter);
    root->addWidget(header);

    auto* grid = new QGridLayout();
    grid->setSpacing(7);
    root->addLayout(grid, 1);

    m_leftTitle = new QLabel(QStringLiteral("左侧玩家"), this);
    m_rightTitle = new QLabel(QStringLiteral("右侧玩家"), this);
    m_leftTitle->setObjectName(QStringLiteral("sideTitle"));
    m_rightTitle->setObjectName(QStringLiteral("sideTitle"));
    m_leftTitle->setAlignment(Qt::AlignCenter);
    m_rightTitle->setAlignment(Qt::AlignCenter);

    m_leftAssets = new QListWidget(this);
    m_rightAssets = new QListWidget(this);
    m_leftOffer = new QListWidget(this);
    m_rightOffer = new QListWidget(this);
    for (QListWidget* list : {m_leftAssets, m_rightAssets, m_leftOffer, m_rightOffer}) {
        list->setMinimumHeight(155);
    }

    m_leftMoney = new QSpinBox(this);
    m_rightMoney = new QSpinBox(this);
    m_leftMoney->setRange(0, 1000000);
    m_rightMoney->setRange(0, 1000000);
    m_leftMoney->setSingleStep(10000);
    m_rightMoney->setSingleStep(10000);
    m_leftMoney->setPrefix(QStringLiteral("¥ "));
    m_rightMoney->setPrefix(QStringLiteral("¥ "));
    for (QSpinBox* spin : {m_leftMoney, m_rightMoney}) {
        spin->setAccelerated(true);
        spin->setFocusPolicy(Qt::WheelFocus);
        spin->setKeyboardTracking(false);
        spin->setToolTip(QStringLiteral("鼠标移到金额框上滚轮调整；每格 10,000，也可键盘输入。"));
    }

    auto makeListBox = [this](const QString& title, QListWidget* list) {
        auto* box = new QGroupBox(title, this);
        auto* layout = new QVBoxLayout(box);
        layout->setContentsMargins(6, 8, 6, 6);
        layout->addWidget(list);
        return box;
    };

    grid->addWidget(m_leftTitle, 0, 0, 1, 2);
    grid->addWidget(m_rightTitle, 0, 2, 1, 2);
    grid->addWidget(makeListBox(QStringLiteral("左方可提供资产"), m_leftAssets), 1, 0);
    grid->addWidget(makeListBox(QStringLiteral("左方本次给出"), m_leftOffer), 1, 1);
    grid->addWidget(makeListBox(QStringLiteral("右方本次给出"), m_rightOffer), 1, 2);
    grid->addWidget(makeListBox(QStringLiteral("右方可提供资产"), m_rightAssets), 1, 3);

    auto makeMoneyBox = [this](const QString& title, QSpinBox* spin) {
        auto* box = new QGroupBox(title, this);
        auto* layout = new QVBoxLayout(box);
        layout->setContentsMargins(6, 8, 6, 6);
        layout->addWidget(spin);

        auto* hint = new QLabel(QStringLiteral("鼠标移到金额框上滚轮调整金额：向上 +10,000，向下 -10,000；也可直接键盘输入。"), box);
        hint->setWordWrap(true);
        hint->setStyleSheet(QStringLiteral("QLabel { color: #ffe6b3; font-size: 12px; font-weight: 600; }"));
        layout->addWidget(hint);
        return box;
    };

    grid->addWidget(makeMoneyBox(QStringLiteral("左方提供金额（上限 1,000,000）"), m_leftMoney), 2, 0, 1, 2);
    grid->addWidget(makeMoneyBox(QStringLiteral("右方提供金额（上限 1,000,000）"), m_rightMoney), 2, 2, 1, 2);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setObjectName(QStringLiteral("summaryLabel"));
    m_summaryLabel->setWordWrap(true);
    m_summaryLabel->setMaximumHeight(118);
    root->addWidget(m_summaryLabel);

    auto* buttonRow = new QHBoxLayout();
    auto* undoButton = new QPushButton(QStringLiteral("撤销"), this);
    auto* clearButton = new QPushButton(QStringLiteral("清空交易内容"), this);
    auto* mapButton = new QPushButton(QStringLiteral("地图选建筑"), this);
    auto* cancelButton = new QPushButton(QStringLiteral("取消"), this);
    m_confirmButton = new QPushButton(QStringLiteral("发起交易"), this);

    buttonRow->addWidget(undoButton);
    buttonRow->addWidget(clearButton);
    buttonRow->addWidget(mapButton);
    buttonRow->addStretch(1);
    buttonRow->addWidget(cancelButton);
    buttonRow->addWidget(m_confirmButton);
    root->addLayout(buttonRow);

    connect(m_leftAssets, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        pushHistory();
        addAssetToOffer(true, itemAsset(item));
    });
    connect(m_rightAssets, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        pushHistory();
        addAssetToOffer(false, itemAsset(item));
    });
    connect(m_leftOffer, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        pushHistory();
        removeAssetFromOffer(true, itemAsset(item));
    });
    connect(m_rightOffer, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        pushHistory();
        removeAssetFromOffer(false, itemAsset(item));
    });

    connect(m_leftMoney, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        m_proposal.leftMoney = value;
        markProposalDirty();
    });
    connect(m_rightMoney, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        m_proposal.rightMoney = value;
        markProposalDirty();
    });

    connect(undoButton, &QPushButton::clicked, this, &TradeDialog::undo);
    connect(clearButton, &QPushButton::clicked, this, &TradeDialog::clearOffer);
    connect(mapButton, &QPushButton::clicked, this, &TradeDialog::openMapTileSelector);
    connect(cancelButton, &QPushButton::clicked, this, &TradeDialog::reject);
    connect(m_confirmButton, &QPushButton::clicked, this, &TradeDialog::confirmTrade);

    refreshCards();
}

void TradeDialog::setController(GameController* controller)
{
    m_controller = controller;
    refreshCards();
}

void TradeDialog::setPlayers(int leftPlayerId, int rightPlayerId)
{
    m_proposal.leftPlayerId = leftPlayerId;
    m_proposal.rightPlayerId = rightPlayerId;
    m_proposal.leftAssets.clear();
    m_proposal.rightAssets.clear();
    m_proposal.leftMoney = 0;
    m_proposal.rightMoney = 0;
    m_history.clear();

    refreshCards();
}

void TradeDialog::setProposal(const TradeProposal& proposal)
{
    m_proposal = proposal;
    m_history.clear();
    refreshCards();
}

QList<TradeAsset> TradeDialog::assetsForPlayer(int playerId) const
{
    QList<TradeAsset> assets;
    if (!m_controller) {
        return assets;
    }

    const Player* player = m_controller->playerById(playerId);
    if (!player) {
        return assets;
    }

    for (int tileId : player->ownedTileIds) {
        assets.append({ TradeAssetType::Tile, tileId, ShopType::None });
    }

    
    for (ShopType type : allShopTypes()) {
        for (int i = 0; i < player->shopCards.value(type, 0); ++i) {
            assets.append({ TradeAssetType::ShopCard, static_cast<int>(type), type });
        }
    }

    return assets;
}

QList<TradeAsset> TradeDialog::filteredAssets(const QList<TradeAsset>& source,
                                              const QList<TradeAsset>& offered) const
{
    QList<TradeAsset> result = source;
    for (const TradeAsset& asset : offered) {
        const int index = result.indexOf(asset);
        if (index >= 0) {
            result.removeAt(index);
        }
    }
    return result;
}

QListWidgetItem* TradeDialog::createAssetItem(const TradeAsset& asset) const
{
    QString text = tradeAssetName(asset);
    if (m_controller && asset.type == TradeAssetType::Tile) {
        const Tile* tile = m_controller->tileById(asset.id);
        if (tile && tile->shopType != ShopType::None) {
            text += QStringLiteral("（包含上面的%1店铺；地块与店铺打包转让）")
                    .arg(shopTypeName(tile->shopType));
        }
    } else if (m_controller && asset.type == TradeAssetType::Shop) {
        const Shop* shop = m_controller->shopById(asset.id);
        if (shop) {
            QStringList tileTexts;
            for (int tileId : shop->tileIds) {
                tileTexts << QString::number(tileId);
            }
            text += QStringLiteral("（位置：%1；旧版独立店铺资产，当前规则不允许单独交易）").arg(tileTexts.join(QStringLiteral(",")));
        }
    }

    auto* item = new QListWidgetItem(text);
    item->setData(AssetTypeRole, static_cast<int>(asset.type));
    item->setData(AssetIdRole, asset.id);
    item->setData(ShopTypeRole, static_cast<int>(asset.shopType));
    return item;
}

TradeAsset TradeDialog::itemAsset(QListWidgetItem* item) const
{
    if (!item) {
        return {};
    }

    TradeAsset asset;
    asset.type = static_cast<TradeAssetType>(item->data(AssetTypeRole).toInt());
    asset.id = item->data(AssetIdRole).toInt();
    asset.shopType = static_cast<ShopType>(item->data(ShopTypeRole).toInt());
    return asset;
}

void TradeDialog::refreshCards()
{
    if (!m_leftAssets || !m_rightAssets) {
        return;
    }

    const Player* left = m_controller ? m_controller->playerById(m_proposal.leftPlayerId) : nullptr;
    const Player* right = m_controller ? m_controller->playerById(m_proposal.rightPlayerId) : nullptr;

    const int localId = m_controller ? m_controller->localPlayerId() : -1;
    m_leftTitle->setText(left ? QStringLiteral("%1%2\n资金：%3  地块：%4  店铺：%5")
                         .arg(left->name)
                         .arg(left->id == localId ? QStringLiteral("（本机）") : QString())
                         .arg(left->id == localId ? QString::number(left->money) : QStringLiteral("隐藏"))
                         .arg(left->ownedTileIds.size())
                         .arg(left->shops.size())
                       : QStringLiteral("左侧玩家"));
    m_rightTitle->setText(right ? QStringLiteral("%1%2\n资金：%3  地块：%4  店铺：%5")
                          .arg(right->name)
                          .arg(right->id == localId ? QStringLiteral("（本机）") : QString())
                          .arg(right->id == localId ? QString::number(right->money) : QStringLiteral("隐藏"))
                          .arg(right->ownedTileIds.size())
                          .arg(right->shops.size())
                        : QStringLiteral("右侧玩家"));

    refreshSide(true);
    refreshSide(false);

    {
        QSignalBlocker blocker(m_leftMoney);
        m_leftMoney->setValue(m_proposal.leftMoney);
    }
    {
        QSignalBlocker blocker(m_rightMoney);
        m_rightMoney->setValue(m_proposal.rightMoney);
    }

    updateValidation();
    refreshSummary();
}

void TradeDialog::refreshSide(bool leftSide)
{
    QListWidget* assetList = leftSide ? m_leftAssets : m_rightAssets;
    QListWidget* offerList = leftSide ? m_leftOffer : m_rightOffer;
    const int playerId = leftSide ? m_proposal.leftPlayerId : m_proposal.rightPlayerId;
    const QList<TradeAsset> offered = leftSide ? m_proposal.leftAssets : m_proposal.rightAssets;

    assetList->clear();
    offerList->clear();

    const QList<TradeAsset> assets = filteredAssets(assetsForPlayer(playerId), offered);
    for (const TradeAsset& asset : assets) {
        assetList->addItem(createAssetItem(asset));
    }

    for (const TradeAsset& asset : offered) {
        offerList->addItem(createAssetItem(asset));
    }
}

void TradeDialog::addAssetToOffer(bool leftSide, const TradeAsset& asset)
{
    if (leftSide) {
        m_proposal.leftAssets.append(asset);
    } else {
        m_proposal.rightAssets.append(asset);
    }

    markProposalDirty();
    refreshCards();
}

void TradeDialog::removeAssetFromOffer(bool leftSide, const TradeAsset& asset)
{
    QList<TradeAsset>& list = leftSide ? m_proposal.leftAssets : m_proposal.rightAssets;
    const int index = list.indexOf(asset);
    if (index >= 0) {
        list.removeAt(index);
    }

    markProposalDirty();
    refreshCards();
}

void TradeDialog::adjustMoney(bool leftSide, int delta)
{
    pushHistory();

    QSpinBox* spin = leftSide ? m_leftMoney : m_rightMoney;
    spin->setValue(std::clamp(spin->value() + delta, 0, 1000000));

    updateMoneyFromWidgets();
    markProposalDirty();
}

void TradeDialog::updateMoneyFromWidgets()
{
    m_proposal.leftMoney = m_leftMoney->value();
    m_proposal.rightMoney = m_rightMoney->value();
}

void TradeDialog::pushHistory()
{
    m_history.append(m_proposal);
    if (m_history.size() > 30) {
        m_history.removeFirst();
    }
}

void TradeDialog::undo()
{
    if (m_history.isEmpty()) {
        return;
    }

    m_proposal = m_history.takeLast();
    refreshCards();
}

void TradeDialog::clearOffer()
{
    pushHistory();

    m_proposal.leftAssets.clear();
    m_proposal.rightAssets.clear();
    m_proposal.leftMoney = 0;
    m_proposal.rightMoney = 0;
    markProposalDirty();
    refreshCards();
}

void TradeDialog::confirmTrade()
{
    updateMoneyFromWidgets();

    QString reason;
    if (!m_controller || !m_controller->validateTrade(m_proposal, &reason)) {
        ToastMessage::showMessage(this, reason);
        AnimationManager::shake(this);
        updateValidation();
        return;
    }

    
    m_proposal.leftReady = true;
    m_proposal.rightReady = false;
    emit tradeConfirmed(m_proposal);
    accept();
}

void TradeDialog::setLeftReady(bool ready)
{
    m_proposal.leftReady = ready;
    refreshSummary();
}

void TradeDialog::setRightReady(bool ready)
{
    m_proposal.rightReady = ready;
    refreshSummary();
}

void TradeDialog::markProposalDirty()
{
    m_proposal.leftReady = false;
    m_proposal.rightReady = false;
    updateValidation();
    refreshSummary();
}

void TradeDialog::lockOfferForPlayer(int playerId)
{
    if (playerId == m_proposal.leftPlayerId) {
        setSideLocked(true, true);
    } else if (playerId == m_proposal.rightPlayerId) {
        setSideLocked(false, true);
    }
}

void TradeDialog::unlockOfferForPlayer(int playerId)
{
    if (playerId == m_proposal.leftPlayerId) {
        setSideLocked(true, false);
    } else if (playerId == m_proposal.rightPlayerId) {
        setSideLocked(false, false);
    }
}

void TradeDialog::setSideLocked(bool leftSide, bool locked)
{
    QListWidget* assets = leftSide ? m_leftAssets : m_rightAssets;
    QListWidget* offer = leftSide ? m_leftOffer : m_rightOffer;
    QSpinBox* money = leftSide ? m_leftMoney : m_rightMoney;

    assets->setEnabled(!locked);
    offer->setEnabled(!locked);
    money->setEnabled(!locked);
}

bool TradeDialog::tileOfferedBySide(bool leftSide, int tileId) const
{
    const QList<TradeAsset>& list = leftSide ? m_proposal.leftAssets : m_proposal.rightAssets;
    return std::any_of(list.cbegin(), list.cend(), [tileId](const TradeAsset& asset) {
        return asset.type == TradeAssetType::Tile && asset.id == tileId;
    });
}

bool TradeDialog::toggleTileOfferFromMap(int tileId)
{
    if (!m_controller) {
        return false;
    }

    const Tile* tile = m_controller->tileById(tileId);
    if (!tile) {
        return false;
    }

    bool leftSide = false;
    if (tile->ownerId == m_proposal.leftPlayerId) {
        leftSide = true;
    } else if (tile->ownerId == m_proposal.rightPlayerId) {
        leftSide = false;
    } else {
        const Player* owner = m_controller->playerById(tile->ownerId);
        ToastMessage::showMessage(this, QStringLiteral("建筑 %1 属于 %2，不在本次交易双方中。")
                                  .arg(tileId)
                                  .arg(owner ? owner->name : QStringLiteral("其他玩家")));
        return false;
    }

    const TradeAsset asset{TradeAssetType::Tile, tileId, ShopType::None};
    pushHistory();
    if (tileOfferedBySide(leftSide, tileId)) {
        removeAssetFromOffer(leftSide, asset);
    } else {
        addAssetToOffer(leftSide, asset);
    }
    return true;
}

void TradeDialog::updateTradeMapEffects(BoardView* view) const
{
    if (!view || !m_controller) {
        return;
    }

    view->clearTileEffects();

    QList<int> tradeableTileIds;
    for (const Tile& tile : m_controller->tiles()) {
        if (tile.ownerId == m_proposal.leftPlayerId || tile.ownerId == m_proposal.rightPlayerId) {
            tradeableTileIds.append(tile.id);
        }
    }

    QList<int> selectedTileIds;
    auto appendSelectedTiles = [&selectedTileIds](const QList<TradeAsset>& assets) {
        for (const TradeAsset& asset : assets) {
            if (asset.type == TradeAssetType::Tile && asset.id > 0 && !selectedTileIds.contains(asset.id)) {
                selectedTileIds.append(asset.id);
            }
        }
    };
    appendSelectedTiles(m_proposal.leftAssets);
    appendSelectedTiles(m_proposal.rightAssets);

    view->showTradeTargetTiles(tradeableTileIds);
    view->setTradeSelectedTiles(selectedTileIds);
}

void TradeDialog::openMapTileSelector()
{
    if (!m_controller) {
        ToastMessage::showMessage(this, QStringLiteral("当前还没有棋局，不能从地图选择建筑。"));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("从实时地图选择交易建筑"));
    dialog.resize(1120, 780);
    dialog.setMinimumSize(920, 640);
    dialog.setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    dialog.setStyleSheet(QStringLiteral(
        "QDialog { background: #241b14; }"
        "QLabel { color: #f7e4b6; font-weight: 700; }"
        "QPushButton { background: #6b3f2b; color: #fff1c4; border: 2px solid #bd9258; border-radius: 9px; padding: 8px 18px; font-weight: 800; }"
        "QPushButton:hover { background: #845036; border-color: #f1d08c; }"));

    auto* root = new QVBoxLayout(&dialog);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    auto* tip = new QLabel(QStringLiteral("蓝框建筑可加入本次交易；橙色勾表示已放入交易框。点击建筑可加入/撤回，完成后点下方按钮。"), &dialog);
    tip->setWordWrap(true);
    root->addWidget(tip);

    auto* view = new BoardView(&dialog);
    view->setMinimumSize(880, 560);
    view->setController(m_controller);
    updateTradeMapEffects(view);
    root->addWidget(view, 1);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->addStretch(1);
    auto* finishButton = new QPushButton(QStringLiteral("完成选择"), &dialog);
    buttonRow->addWidget(finishButton);
    root->addLayout(buttonRow);

    connect(view, &BoardView::tileClicked, &dialog, [this, view](int tileId) {
        if (toggleTileOfferFromMap(tileId)) {
            updateTradeMapEffects(view);
        }
    });
    connect(m_controller, &GameController::stateChanged, view, [this, view] {
        view->rebuildBoard();
        updateTradeMapEffects(view);
    });
    connect(finishButton, &QPushButton::clicked, &dialog, &QDialog::accept);

    dialog.exec();
    refreshCards();
}

void TradeDialog::updateValidation()
{
    QString reason;
    const bool ok = m_controller && m_controller->validateTrade(m_proposal, &reason);
    m_confirmButton->setEnabled(ok);
    m_confirmButton->setToolTip(reason);
}

void TradeDialog::refreshSummary()
{
    if (!m_summaryLabel) {
        return;
    }

    auto summarize = [](const QList<TradeAsset>& assets) {
        QStringList names;
        for (const TradeAsset& asset : assets) {
            names << tradeAssetName(asset);
        }
        return names.isEmpty() ? QStringLiteral("无资产") : names.join(QStringLiteral("、"));
    };

    QString reason;
    const bool ok = m_controller && m_controller->validateTrade(m_proposal, &reason);

    m_summaryLabel->setText(QStringLiteral(
        "【交易内容预览】\n"
        "左方给出：%1\n"
        "左方金额：¥ %2\n"
        "右方给出：%3\n"
        "右方金额：¥ %4\n"
        "\n【确认规则】双方都需要在自己的界面点击同意。已建店铺会随所在地块一起打包转让。\n"
        "【当前校验】%5")
        .arg(summarize(m_proposal.leftAssets))
        .arg(m_proposal.leftMoney)
        .arg(summarize(m_proposal.rightAssets))
        .arg(m_proposal.rightMoney)
        .arg(ok ? QStringLiteral("可以发送给对方确认") : reason));
}
