#include "ui/BuildDialog.h"

#include "core/GameController.h"
#include "ui/ToastMessage.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHeaderView>
#include <QMap>
#include <QSet>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <utility>

BuildDialog::BuildDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("批量建设店铺"));
    resize(720, 560);
    setStyleSheet(QStringLiteral(
        "QDialog { background: #2f281f; }"
        "QGroupBox { color: #fce7b2; border: 1px solid #7c6a52; border-radius: 8px; margin-top: 8px; padding-top: 8px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"
        "QLabel { color: #fce7b2; }"
        "QTableWidget { background: #fbf3dc; color: #3d3329; border: 1px solid #9a8465; border-radius: 6px; gridline-color: #d4bd8e; }"
        "QHeaderView::section { background: #d9c08e; color: #3d3329; padding: 5px; border: 0px; }"
        "QComboBox { background: #fff8e5; color: #3d3329; border: 1px solid #9a8465; border-radius: 4px; padding: 3px; }"
        "QPushButton { background: #6b3f2b; color: #fff1c4; border: 1px solid #b58a4a; border-radius: 6px; padding: 7px 12px; }"
        "QPushButton:disabled { background: #4c4338; color: #9d9282; border-color: #5d5247; }"));

    auto* layout = new QVBoxLayout(this);

    auto* batchGroup = new QGroupBox(QStringLiteral("批量建设：一次选择多个可建设店铺"), this);
    auto* batchLayout = new QVBoxLayout(batchGroup);
    auto* batchTip = new QLabel(QStringLiteral(
        "只保留一个批量建设按钮：在要建设的地块行中勾选“建设”，再选择店铺类型。"
        "本阶段不强制建设；不想建设时直接点击“跳过/完成建设”。系统会自动检查店铺卡数量与空地块。"), batchGroup);
    batchTip->setWordWrap(true);
    batchLayout->addWidget(batchTip);

    m_batchTable = new QTableWidget(batchGroup);
    m_batchTable->setColumnCount(3);
    m_batchTable->setHorizontalHeaderLabels(QStringList() << QStringLiteral("建设") << QStringLiteral("地块") << QStringLiteral("店铺类型"));
    m_batchTable->horizontalHeader()->setStretchLastSection(true);
    m_batchTable->verticalHeader()->setVisible(false);
    m_batchTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_batchTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_batchTable->setMinimumHeight(300);
    batchLayout->addWidget(m_batchTable, 1);

    m_batchPreviewLabel = new QLabel(batchGroup);
    m_batchPreviewLabel->setWordWrap(true);
    m_batchPreviewLabel->setStyleSheet(QStringLiteral("background: #fbf3dc; color: #3d3329; border: 1px solid #9a8465; border-radius: 6px; padding: 8px;"));
    batchLayout->addWidget(m_batchPreviewLabel);

    auto* buttonRow = new QHBoxLayout();
    auto* finishButton = new QPushButton(QStringLiteral("跳过/完成建设"), batchGroup);
    m_batchConfirmButton = new QPushButton(QStringLiteral("批量建设所选方案"), batchGroup);
    finishButton->setMinimumHeight(36);
    m_batchConfirmButton->setMinimumHeight(36);
    buttonRow->addWidget(finishButton);
    buttonRow->addStretch(1);
    buttonRow->addWidget(m_batchConfirmButton);
    batchLayout->addLayout(buttonRow);

    layout->addWidget(batchGroup, 1);

    connect(m_batchConfirmButton, &QPushButton::clicked, this, &BuildDialog::confirmBatchBuild);
    connect(m_batchTable, &QTableWidget::itemChanged, this, &BuildDialog::refreshPreview);
    connect(finishButton, &QPushButton::clicked, this, &BuildDialog::reject);

    refreshPreview();
}

void BuildDialog::setController(GameController* controller)
{
    m_controller = controller;
    refreshShopChoices();
    refreshTileChoices();
    refreshBatchChoices();
    refreshPreview();
}

void BuildDialog::setPlayerId(int playerId)
{
    m_playerId = playerId;
    refreshShopChoices();
    refreshTileChoices();
    refreshBatchChoices();
    refreshPreview();
}

void BuildDialog::setCandidateTiles(const QList<int>& tileIds)
{
    m_candidateTiles = tileIds;
    refreshTileChoices();
    refreshBatchChoices();
    refreshPreview();
}

void BuildDialog::refreshShopChoices()
{
    if (!m_shopCombo) {
        return;
    }

    const QSignalBlocker blocker(m_shopCombo);
    m_shopCombo->clear();

    if (!m_controller) {
        return;
    }

    const Player* player = m_controller->playerById(m_playerId);
    if (!player) {
        return;
    }

    for (ShopType type : allShopTypes()) {
        const int count = player->shopCards.value(type, 0);
        if (count > 0) {
            m_shopCombo->addItem(QStringLiteral("%1（剩余 %2）").arg(shopTypeName(type)).arg(count),
                                 static_cast<int>(type));
        }
    }
}

void BuildDialog::refreshTileChoices()
{
    if (!m_tileList) {
        return;
    }

    const QSignalBlocker blocker(m_tileList);
    m_tileList->clear();

    for (int tileId : std::as_const(m_candidateTiles)) {
        auto* item = new QListWidgetItem(QStringLiteral("地块 %1").arg(tileId), m_tileList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        item->setData(Qt::UserRole, tileId);
    }
}

void BuildDialog::refreshBatchChoices()
{
    if (!m_batchTable) {
        return;
    }

    const QSignalBlocker blocker(m_batchTable);
    m_batchTable->clearContents();
    m_batchTable->setRowCount(m_candidateTiles.size());

    const Player* player = m_controller ? m_controller->playerById(m_playerId) : nullptr;

    for (int row = 0; row < m_candidateTiles.size(); ++row) {
        const int tileId = m_candidateTiles.at(row);

        auto* check = new QTableWidgetItem;
        check->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        check->setCheckState(Qt::Unchecked);
        check->setData(Qt::UserRole, tileId);
        m_batchTable->setItem(row, 0, check);

        auto* tileItem = new QTableWidgetItem(QStringLiteral("地块 %1").arg(tileId));
        tileItem->setData(Qt::UserRole, tileId);
        m_batchTable->setItem(row, 1, tileItem);

        auto* combo = new QComboBox(m_batchTable);
        combo->addItem(QStringLiteral("不建设"), static_cast<int>(ShopType::None));
        if (player) {
            for (ShopType type : allShopTypes()) {
                const int count = player->shopCards.value(type, 0);
                if (count > 0) {
                    combo->addItem(QStringLiteral("%1（剩余 %2）").arg(shopTypeName(type)).arg(count),
                                   static_cast<int>(type));
                }
            }
        }
        connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this, &BuildDialog::refreshPreview);
        m_batchTable->setCellWidget(row, 2, combo);
    }

    m_batchTable->resizeColumnsToContents();
}

ShopType BuildDialog::selectedShopType() const
{
    if (!m_shopCombo || m_shopCombo->currentIndex() < 0) {
        return ShopType::None;
    }
    return static_cast<ShopType>(m_shopCombo->currentData().toInt());
}

QList<int> BuildDialog::selectedTileIds() const
{
    QList<int> ids;
    for (int i = 0; i < m_tileList->count(); ++i) {
        QListWidgetItem* item = m_tileList->item(i);
        if (item->checkState() == Qt::Checked) {
            ids.append(item->data(Qt::UserRole).toInt());
        }
    }
    return ids;
}

QList<QPair<ShopType, int>> BuildDialog::selectedBatchPlans(QString* errorMessage) const
{
    QList<QPair<ShopType, int>> plans;
    if (!m_batchTable) {
        return plans;
    }

    const Player* player = m_controller ? m_controller->playerById(m_playerId) : nullptr;
    if (!player) {
        if (errorMessage) *errorMessage = QStringLiteral("玩家不存在");
        return plans;
    }

    QSet<int> usedTiles;
    QMap<ShopType, int> needCards;

    for (int row = 0; row < m_batchTable->rowCount(); ++row) {
        QTableWidgetItem* check = m_batchTable->item(row, 0);
        if (!check || check->checkState() != Qt::Checked) {
            continue;
        }

        const int tileId = check->data(Qt::UserRole).toInt();
        auto* combo = qobject_cast<QComboBox*>(m_batchTable->cellWidget(row, 2));
        const ShopType type = combo ? static_cast<ShopType>(combo->currentData().toInt()) : ShopType::None;

        if (type == ShopType::None) {
            if (errorMessage) *errorMessage = QStringLiteral("已勾选地块 %1，但没有选择店铺类型").arg(tileId);
            return {};
        }

        if (usedTiles.contains(tileId)) {
            if (errorMessage) *errorMessage = QStringLiteral("地块 %1 被重复选择").arg(tileId);
            return {};
        }
        usedTiles.insert(tileId);

        needCards[type] = needCards.value(type, 0) + 1;
        if (needCards.value(type) > player->shopCards.value(type, 0)) {
            if (errorMessage) *errorMessage = QStringLiteral("%1 店铺卡不足，需要 %2 张，当前只有 %3 张")
                                     .arg(shopTypeName(type))
                                     .arg(needCards.value(type))
                                     .arg(player->shopCards.value(type, 0));
            return {};
        }

        QString reason;
        if (!m_controller->canBuildShop(m_playerId, type, QList<int>{tileId}, &reason)) {
            if (errorMessage) *errorMessage = reason;
            return {};
        }

        plans.append(qMakePair(type, tileId));
    }

    if (plans.isEmpty() && errorMessage) {
        *errorMessage = QStringLiteral("请至少勾选一个要建设的地块并选择店铺类型");
    }

    return plans;
}

void BuildDialog::refreshPreview()
{
    if (m_previewLabel && m_confirmButton) {
        const ShopType type = selectedShopType();
        const QList<int> tiles = selectedTileIds();

        QString reason;
        const bool ok = m_controller &&
                m_controller->canBuildShop(m_playerId, type, tiles, &reason);

        const int estimatedIncome = ok ? (tiles.size() * 1000) : 0;
        QStringList idTexts;
        for (int id : tiles) {
            idTexts << QString::number(id);
        }

        m_previewLabel->setText(QStringLiteral(
            "店铺：%1\n"
            "地块：%2\n"
            "状态：%3\n"
            "预估基础收入：%4")
            .arg(shopTypeName(type))
            .arg(idTexts.isEmpty() ? QStringLiteral("未选择") : idTexts.join(QStringLiteral(", ")))
            .arg(reason)
            .arg(estimatedIncome));

        m_confirmButton->setEnabled(ok);
    }

    if (m_batchPreviewLabel && m_batchConfirmButton) {
        QString batchReason;
        const QList<QPair<ShopType, int>> plans = selectedBatchPlans(&batchReason);
        const bool batchOk = !plans.isEmpty();
        QStringList texts;
        for (const auto& plan : plans) {
            texts << QStringLiteral("地块 %1→%2").arg(plan.second).arg(shopTypeName(plan.first));
        }
        m_batchPreviewLabel->setText(batchOk
            ? QStringLiteral("将一次建设 %1 个店铺：%2").arg(plans.size()).arg(texts.join(QStringLiteral("；")))
            : QStringLiteral("批量状态：%1").arg(batchReason));
        m_batchConfirmButton->setEnabled(batchOk);
    }
}

void BuildDialog::confirmBuild()
{
    const ShopType type = selectedShopType();
    const QList<int> tiles = selectedTileIds();

    QString reason;
    if (!m_controller || !m_controller->canBuildShop(m_playerId, type, tiles, &reason)) {
        ToastMessage::showMessage(this, reason);
        refreshPreview();
        return;
    }

    emit buildConfirmed(type, tiles);

    if (m_controller) {
        m_candidateTiles = m_controller->buildableTilesForPlayer(m_playerId);
    }
    refreshShopChoices();
    refreshTileChoices();
    refreshBatchChoices();
    refreshPreview();

    if (!m_controller || !m_controller->playerHasBuildOptions(m_playerId)) {
        ToastMessage::showMessage(this, QStringLiteral("该玩家已没有可继续建设的店铺卡片或空建筑。"));
        accept();
    }
}

void BuildDialog::confirmBatchBuild()
{
    QString reason;
    const QList<QPair<ShopType, int>> plans = selectedBatchPlans(&reason);
    if (plans.isEmpty()) {
        ToastMessage::showMessage(this, reason);
        refreshPreview();
        return;
    }

    for (const auto& plan : plans) {
        emit buildConfirmed(plan.first, QList<int>{plan.second});
    }

    ToastMessage::showMessage(this, QStringLiteral("已批量建设 %1 个店铺。可继续建设或点击跳过/完成建设。").arg(plans.size()));

    if (m_controller) {
        m_candidateTiles = m_controller->buildableTilesForPlayer(m_playerId);
    }
    refreshShopChoices();
    refreshTileChoices();
    refreshBatchChoices();
    refreshPreview();

    if (!m_controller || !m_controller->playerHasBuildOptions(m_playerId)) {
        ToastMessage::showMessage(this, QStringLiteral("该玩家已没有可继续建设的店铺卡片或空建筑。"));
        accept();
    }
}
