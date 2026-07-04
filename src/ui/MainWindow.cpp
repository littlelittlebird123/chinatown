#include "ui/MainWindow.h"

#include "core/GameController.h"
#include "ui/ActionPanel.h"
#include "ui/BoardView.h"
#include "ui/BoardTheme.h"
#include "ui/BuildDialog.h"
#include "ui/GameLogPanel.h"
#include "ui/HandPanel.h"
#include "ui/PlayerPanel.h"
#include "ui/TileInfoPanel.h"
#include "ui/ToastMessage.h"
#include "ui/TradeDialog.h"
#include "network/NetworkGameBridge.h"
#include "server/GameServer.h"
#include "utils/AssetManager.h"

#include <QAbstractAnimation>
#include <QAction>
#include <QAbstractButton>
#include <QComboBox>
#include <QColor>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QAudioOutput>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListView>
#include <QMediaPlayer>
#include <QMenuBar>
#include <QMap>
#include <QMessageBox>
#include <QPushButton>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QScrollArea>
#include <QScrollBar>
#include <QSize>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QStringList>
#include <QStatusBar>
#include <QTextCursor>
#include <QTextEdit>
#include <QUrl>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <utility>

namespace {

QJsonObject assetToJson(const TradeAsset& asset)
{
    QJsonObject obj;
    obj[QStringLiteral("type")] = static_cast<int>(asset.type);
    obj[QStringLiteral("id")] = asset.id;
    obj[QStringLiteral("shopType")] = static_cast<int>(asset.shopType);
    return obj;
}

TradeAsset assetFromJson(const QJsonObject& obj)
{
    TradeAsset asset;
    asset.type = static_cast<TradeAssetType>(obj.value(QStringLiteral("type")).toInt(static_cast<int>(TradeAssetType::Tile)));
    asset.id = obj.value(QStringLiteral("id")).toInt(-1);
    asset.shopType = static_cast<ShopType>(obj.value(QStringLiteral("shopType")).toInt(static_cast<int>(ShopType::None)));
    return asset;
}

QJsonObject proposalToJson(const TradeProposal& proposal)
{
    QJsonObject obj;
    obj[QStringLiteral("leftPlayerId")] = proposal.leftPlayerId;
    obj[QStringLiteral("rightPlayerId")] = proposal.rightPlayerId;
    obj[QStringLiteral("leftMoney")] = proposal.leftMoney;
    obj[QStringLiteral("rightMoney")] = proposal.rightMoney;
    obj[QStringLiteral("leftReady")] = proposal.leftReady;
    obj[QStringLiteral("rightReady")] = proposal.rightReady;

    QJsonArray leftAssets;
    for (const TradeAsset& asset : proposal.leftAssets) {
        leftAssets.append(assetToJson(asset));
    }
    obj[QStringLiteral("leftAssets")] = leftAssets;

    QJsonArray rightAssets;
    for (const TradeAsset& asset : proposal.rightAssets) {
        rightAssets.append(assetToJson(asset));
    }
    obj[QStringLiteral("rightAssets")] = rightAssets;
    return obj;
}

TradeProposal proposalFromJson(const QJsonObject& obj)
{
    TradeProposal proposal;
    proposal.leftPlayerId = obj.value(QStringLiteral("leftPlayerId")).toInt(-1);
    proposal.rightPlayerId = obj.value(QStringLiteral("rightPlayerId")).toInt(-1);
    proposal.leftMoney = obj.value(QStringLiteral("leftMoney")).toInt(0);
    proposal.rightMoney = obj.value(QStringLiteral("rightMoney")).toInt(0);
    proposal.leftReady = obj.value(QStringLiteral("leftReady")).toBool(false);
    proposal.rightReady = obj.value(QStringLiteral("rightReady")).toBool(false);

    const QJsonArray leftAssets = obj.value(QStringLiteral("leftAssets")).toArray();
    for (const QJsonValue& value : leftAssets) {
        proposal.leftAssets.append(assetFromJson(value.toObject()));
    }

    const QJsonArray rightAssets = obj.value(QStringLiteral("rightAssets")).toArray();
    for (const QJsonValue& value : rightAssets) {
        proposal.rightAssets.append(assetFromJson(value.toObject()));
    }
    return proposal;
}

QString assetsSummary(const QList<TradeAsset>& assets)
{
    QStringList names;
    for (const TradeAsset& asset : assets) {
        names << tradeAssetName(asset);
    }
    return names.isEmpty() ? QStringLiteral("无资产") : names.join(QStringLiteral("、"));
}

QString proposalSummary(const GameController* controller, const TradeProposal& proposal)
{
    const Player* left = controller ? controller->playerById(proposal.leftPlayerId) : nullptr;
    const Player* right = controller ? controller->playerById(proposal.rightPlayerId) : nullptr;
    return QStringLiteral("%1 提供：%2，金额：%3\n%4 提供：%5，金额：%6")
        .arg(left ? left->name : QStringLiteral("左方"))
        .arg(assetsSummary(proposal.leftAssets))
        .arg(proposal.leftMoney)
        .arg(right ? right->name : QStringLiteral("右方"))
        .arg(assetsSummary(proposal.rightAssets))
        .arg(proposal.rightMoney);
}

QString makeProposalId(int leftPlayerId, int rightPlayerId)
{
    return QStringLiteral("T%1_%2_%3")
        .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(leftPlayerId)
        .arg(rightPlayerId);
}

TradeProposal swappedProposal(const TradeProposal& proposal)
{
    TradeProposal swapped;
    swapped.leftPlayerId = proposal.rightPlayerId;
    swapped.rightPlayerId = proposal.leftPlayerId;
    swapped.leftAssets = proposal.rightAssets;
    swapped.rightAssets = proposal.leftAssets;
    swapped.leftMoney = proposal.rightMoney;
    swapped.rightMoney = proposal.leftMoney;
    swapped.leftReady = false;
    swapped.rightReady = false;
    return swapped;
}

} 

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_controller(new GameController(this))
    , m_networkBridge(new NetworkGameBridge(m_controller, this))
    , m_localServer(new GameServer(this))
{
    setWindowTitle(QStringLiteral("唐人街桌游 Qt Demo"));
    setWindowOpacity(0.0);

    setupUi();
    setupConnections();
    initializeBgm();
    updateBgmForCurrentRound();

    QTimer::singleShot(0, this, &MainWindow::showStartupDialog);
}

void MainWindow::setupUi()
{
    resize(1380, 860);
    setMinimumSize(1220, 760);

    auto* central = new QWidget(this);
    central->setObjectName(QStringLiteral("centralRoot"));
    central->setStyleSheet(QStringLiteral(
        "#centralRoot { background: #2f281f; }"
        "QSplitter::handle { background: #514536; }"
        "QGroupBox { color: #fce7b2; border: 1px solid #7c6a52; border-radius: 8px; margin-top: 8px; padding-top: 8px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"
        "QPushButton { background: #6b3f2b; color: #fff1c4; border: 1px solid #b58a4a; border-radius: 6px; padding: 7px 12px; }"
        "QPushButton:hover { background: #815037; }"
        "QPushButton:disabled { background: #4c4338; color: #9d9282; border-color: #5d5247; }"));

    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(10, 8, 10, 8);
    root->setSpacing(8);

    auto* header = new QWidget(central);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(8);

    m_topBar = new QLabel(this);
    m_topBar->setMinimumHeight(44);
    m_topBar->setAlignment(Qt::AlignCenter);
    m_topBar->setStyleSheet(QStringLiteral(
        "QLabel { background: #392f25; color: #fce7b2; font-weight: bold; font-size: 17px; border-radius: 8px; padding: 6px; }"));
    headerLayout->addWidget(m_topBar, 1);

    m_incomeButton = new QPushButton(QStringLiteral("连锁收入表"), header);
    m_incomeButton->setMinimumHeight(42);
    m_incomeButton->setToolTip(QStringLiteral("点击弹出各商铺连锁收入表"));
    headerLayout->addWidget(m_incomeButton);

    m_logButton = new QPushButton(QStringLiteral("信息记录"), header);
    m_logButton->setMinimumHeight(42);
    m_logButton->setToolTip(QStringLiteral("弹出游戏日志窗口"));
    headerLayout->addWidget(m_logButton);

    m_bgmVolumeButton = new QPushButton(QStringLiteral("BGM音量 45%"), header);
    m_bgmVolumeButton->setMinimumHeight(42);
    m_bgmVolumeButton->setToolTip(QStringLiteral("点击将 BGM 音量调小 10%；到 0% 后再次点击恢复 45%。"));
    headerLayout->addWidget(m_bgmVolumeButton);
    root->addWidget(header);

    m_phaseBanner = new QLabel(QStringLiteral("准备进入游戏"), central);
    m_phaseBanner->setAlignment(Qt::AlignCenter);
    m_phaseBanner->setFixedHeight(42);
    m_phaseBanner->setStyleSheet(QStringLiteral(
        "QLabel { background: rgba(170, 95, 32, 190); color: white; font-size: 18px; font-weight: bold; border-radius: 10px; }"));
    root->addWidget(m_phaseBanner);

    auto* horizontalSplitter = new QSplitter(Qt::Horizontal, this);
    horizontalSplitter->setChildrenCollapsible(false);

    auto* leftPanel = new QWidget(horizontalSplitter);
    leftPanel->setMinimumWidth(320);
    leftPanel->setMaximumWidth(420);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(4, 4, 6, 4);
    leftLayout->setSpacing(10);

    m_playerPanel = new PlayerPanel(leftPanel);
    leftLayout->addWidget(m_playerPanel, 1);

    m_tileInfoPanel = new TileInfoPanel(leftPanel);
    m_tileInfoPanel->setMinimumHeight(100);
    m_tileInfoPanel->setStyleSheet(QStringLiteral(
        "TileInfoPanel { background: #45392d; border: 1px solid #7c6a52; border-radius: 8px; color: #f7e6bd; }"
        "QLabel { color: #f7e6bd; font-size: 14px; }"));
    leftLayout->addWidget(m_tileInfoPanel, 0);

    m_actionPanel = new ActionPanel(leftPanel);
    m_actionPanel->setStyleSheet(QStringLiteral(
        "ActionPanel { background: #45392d; border: 1px solid #7c6a52; border-radius: 8px; color: #f7e6bd; }"));
    leftLayout->addWidget(m_actionPanel, 0);

    auto* chatBox = new QGroupBox(QStringLiteral("交易聊天"), leftPanel);
    chatBox->setMinimumHeight(170);
    chatBox->setStyleSheet(QStringLiteral(
        "QGroupBox { background: #203A3F; color: #f4eadb; border: 1px solid #D8B56A; border-radius: 8px; margin-top: 8px; padding-top: 10px; font-weight: bold; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"));
    auto* chatLayout = new QVBoxLayout(chatBox);
    chatLayout->setContentsMargins(8, 10, 8, 8);
    chatLayout->setSpacing(6);

    m_chatHistory = new QTextEdit(chatBox);
    m_chatHistory->setReadOnly(true);
    m_chatHistory->setPlaceholderText(QStringLiteral("交易阶段的聊天消息会显示在这里。"));
    m_chatHistory->setStyleSheet(QStringLiteral(
        "QTextEdit { background: #F4EADB; color: #2C332F; border: 1px solid #D8B56A; border-radius: 6px; padding: 6px; font-size: 12px; }"));
    chatLayout->addWidget(m_chatHistory, 1);

    auto* chatButtonRow = new QHBoxLayout();
    chatButtonRow->addStretch(1);
    m_chatSendButton = new QPushButton(QStringLiteral("发送消息"), chatBox);
    m_chatSendButton->setMinimumHeight(32);
    m_chatSendButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: #8F312A; color: #FFF8EC; border: 2px solid #D8B56A; border-radius: 8px; padding: 5px 12px; font-weight: bold; }"
        "QPushButton:hover { background: #9B2F28; }"));
    chatButtonRow->addWidget(m_chatSendButton);
    chatLayout->addLayout(chatButtonRow);
    leftLayout->addWidget(chatBox, 0);

    m_endTradeButton = new QPushButton(QStringLiteral("结束当前阶段操作"), leftPanel);
    m_endTradeButton->setMinimumHeight(48);
    m_endTradeButton->setToolTip(QStringLiteral("抽地块阶段用于确认选择；交易阶段表示我已结束交易；建设阶段表示本玩家不再继续建设"));
    leftLayout->addWidget(m_endTradeButton, 0);

    m_boardView = new BoardView(horizontalSplitter);
    m_boardView->setMinimumSize(1160, 760);
    m_boardView->setStyleSheet(QStringLiteral("QGraphicsView { border: 2px solid #8f7d62; border-radius: 10px; background: #4c4134; }"));

    horizontalSplitter->addWidget(leftPanel);
    horizontalSplitter->addWidget(m_boardView);
    horizontalSplitter->setStretchFactor(0, 0);
    horizontalSplitter->setStretchFactor(1, 1);
    horizontalSplitter->setSizes(QList<int>() << 360 << 1280);

    root->addWidget(horizontalSplitter, 1);

    setCentralWidget(central);

    m_incomeDialog = new QDialog(this);
    m_incomeDialog->setWindowTitle(QStringLiteral("各商铺连锁收入表"));
    m_incomeDialog->resize(560, 420);
    m_incomeDialog->setStyleSheet(QStringLiteral(
        "QDialog { background: #2f281f; }"
        "QLabel { color: #f7e6bd; background: #3b3128; border-radius: 8px; padding: 8px; font-size: 12px; }"
        "QPushButton { background: #6b3f2b; color: #fff1c4; border: 1px solid #b58a4a; border-radius: 6px; padding: 7px 12px; }"));
    auto* incomeRoot = new QVBoxLayout(m_incomeDialog);
    auto* incomeLabel = new QLabel(m_incomeDialog);
    incomeLabel->setTextFormat(Qt::RichText);
    incomeLabel->setText(QStringLiteral(R"HTML(
<table cellspacing="2" cellpadding="4">
<tr><th>商铺</th><th>满编</th><th>3连</th><th>4连</th><th>5连</th><th>6连</th></tr>
<tr><td>照相馆</td><td>3</td><td>5万</td><td>6万</td><td>7万</td><td>10万</td></tr>
<tr><td>茶馆</td><td>3</td><td>5万</td><td>6万</td><td>7万</td><td>10万</td></tr>
<tr><td>海鲜店</td><td>3</td><td>5万</td><td>6万</td><td>7万</td><td>10万</td></tr>
<tr><td>珠宝店</td><td>4</td><td>4万</td><td>8万</td><td>9万</td><td>10万</td></tr>
<tr><td>观赏鱼店</td><td>4</td><td>4万</td><td>8万</td><td>9万</td><td>10万</td></tr>
<tr><td>鲜花店</td><td>4</td><td>4万</td><td>8万</td><td>9万</td><td>10万</td></tr>
<tr><td>外卖店</td><td>5</td><td>4万</td><td>6万</td><td>11万</td><td>12万</td></tr>
<tr><td>洗衣店</td><td>5</td><td>4万</td><td>6万</td><td>11万</td><td>12万</td></tr>
<tr><td>小吃店</td><td>5</td><td>4万</td><td>6万</td><td>11万</td><td>12万</td></tr>
<tr><td>古董店</td><td>6</td><td>4万</td><td>6万</td><td>8万</td><td>14万</td></tr>
<tr><td>裁缝店</td><td>6</td><td>4万</td><td>6万</td><td>8万</td><td>14万</td></tr>
<tr><td>餐馆</td><td>6</td><td>4万</td><td>6万</td><td>8万</td><td>14万</td></tr>
</table>
<div>说明：一连=1万、二连=2万已省略。只统计上下左右相邻的同类店铺；超出满编规模时按新组继续累计。</div>
)HTML"));
    incomeLabel->setWordWrap(true);
    incomeRoot->addWidget(incomeLabel, 1);
    auto* incomeClose = new QPushButton(QStringLiteral("关闭"), m_incomeDialog);
    incomeRoot->addWidget(incomeClose, 0, Qt::AlignRight);
    connect(incomeClose, &QPushButton::clicked, m_incomeDialog, &QDialog::hide);

    m_logDialog = new QDialog(this);
    m_logDialog->setWindowTitle(QStringLiteral("信息记录"));
    m_logDialog->resize(620, 520);
    m_logDialog->setStyleSheet(QStringLiteral(
        "QDialog { background: #2f281f; }"
        "QTextEdit { background: #fbf3dc; color: #3d3329; border: 1px solid #9a8465; border-radius: 8px; padding: 8px; }"));
    auto* logLayout = new QVBoxLayout(m_logDialog);
    auto* logTip = new QLabel(QStringLiteral("游戏流程、联网同步与交易记录"), m_logDialog);
    logTip->setStyleSheet(QStringLiteral("QLabel { color: #fce7b2; font-weight: bold; font-size: 15px; }"));
    logLayout->addWidget(logTip);
    m_logPanel = new GameLogPanel(m_logDialog);
    logLayout->addWidget(m_logPanel, 1);

    if (menuBar()) {
        menuBar()->hide();
    }

    connect(m_logButton, &QPushButton::clicked, this, &MainWindow::openLogDialog);
    connect(m_incomeButton, &QPushButton::clicked, this, [this] {
        if (!m_incomeDialog) {
            return;
        }
        const QPoint topRight = mapToGlobal(rect().topRight());
        m_incomeDialog->move(topRight.x() - m_incomeDialog->width() - 18, topRight.y() + 72);
        m_incomeDialog->show();
        m_incomeDialog->raise();
        m_incomeDialog->activateWindow();
    });

    m_playerPanel->setController(m_controller);
    m_boardView->setController(m_controller);
    m_tileInfoPanel->setController(m_controller);

    statusBar()->hide();
    updateTradeEndUi();
}

void MainWindow::setupConnections()
{
    connect(m_boardView, &BoardView::tileClicked,
            this, &MainWindow::handleBoardTileClicked);

    connect(m_actionPanel, &ActionPanel::tradeRequested,
            this, &MainWindow::openTradeDialog);

    connect(m_actionPanel, &ActionPanel::buildRequested,
            this, &MainWindow::openBuildDialog);

    if (m_chatSendButton) {
        connect(m_chatSendButton, &QPushButton::clicked,
                this, &MainWindow::openChatMessageDialog);
    }
    if (m_bgmVolumeButton) {
        connect(m_bgmVolumeButton, &QPushButton::clicked,
                this, &MainWindow::reduceBgmVolume);
    }

    connect(m_endTradeButton, &QPushButton::clicked,
            this, [this] {
                switch (m_controller->phase()) {
                case GamePhase::DrawTile:
                    confirmTileChoicesFromMap();
                    break;
                case GamePhase::Trade:
                    requestEndTradeForLocalPlayer();
                    break;
                case GamePhase::Build:
                    finishBuildForLocalPlayer();
                    break;
                default:
                    ToastMessage::showMessage(this, QStringLiteral("当前阶段无需手动确认。"));
                    break;
                }
            });

    connect(m_controller, &GameController::stateChanged,
            this, &MainWindow::refreshAll);

    connect(m_controller, &GameController::tileUpdated,
            m_boardView, &BoardView::refreshTile);

    connect(m_controller, &GameController::phaseChanged,
            m_boardView, &BoardView::rebuildBoard);

    connect(m_controller, &GameController::phaseChanged,
            this, &MainWindow::showPhaseTransition);

    connect(m_controller, &GameController::phaseChanged,
            this, [this](GamePhase) {
                updateBgmForCurrentRound();
            });

    connect(m_controller, &GameController::stateChanged,
            this, [this] {
                updateBgmForCurrentRound();
            });

    connect(m_controller, &GameController::playerUpdated,
            m_playerPanel, &PlayerPanel::refreshPlayer);

    connect(m_controller, &GameController::tileSelected,
            m_boardView, &BoardView::highlightTile);

    connect(m_controller, &GameController::tileSelected,
            m_tileInfoPanel, &TileInfoPanel::showTile);

    connect(m_controller, &GameController::messageGenerated,
            m_logPanel, &GameLogPanel::appendMessage);

    connect(m_controller, &GameController::messageGenerated,
            this, [this](const QString& message) {
                statusBar()->showMessage(message, 3000);
            });

    connect(m_networkBridge, &NetworkGameBridge::networkMessageGenerated,
            m_logPanel, &GameLogPanel::appendMessage);

    connect(m_networkBridge, &NetworkGameBridge::networkMessageGenerated,
            this, [this](const QString& message) {
                statusBar()->showMessage(message, 4000);
            });

    connect(m_networkBridge, &NetworkGameBridge::chatReceived,
            this, [this](int boundPlayerId, const QString& playerName, const QString& target, const QString& text) {
                const bool outgoing = m_controller && boundPlayerId == m_controller->localPlayerId();
                appendChatMessage(outgoing ? QStringLiteral("我") : playerName,
                                  target,
                                  text,
                                  outgoing);
            });

    connect(m_networkBridge, &NetworkGameBridge::roomJoined,
            this, &MainWindow::handleNetworkRoomJoined);

    connect(m_networkBridge, &NetworkGameBridge::remoteTileChoiceRequested,
            this, [this](int playerId, QList<int> candidateTileIds, int chooseCount) {
                if (playerId == m_controller->localPlayerId()) {
                    handleTileChoiceRequested(playerId, candidateTileIds, chooseCount);
                } else if (m_logPanel) {
                    const Player* player = m_controller->playerById(playerId);
                    m_logPanel->appendMessage(QStringLiteral("等待 %1 选择地块。")
                                              .arg(player ? player->name : QStringLiteral("其他玩家")));
                }
            });

    connect(m_networkBridge, &NetworkGameBridge::remoteBuildChoiceRequested,
            this, [this](int playerId) {
                if (playerId == m_controller->localPlayerId()) {
                    handleBuildChoiceRequested(playerId);
                } else if (m_logPanel) {
                    const Player* player = m_controller->playerById(playerId);
                    m_logPanel->appendMessage(QStringLiteral("等待 %1 建设店铺。")
                                              .arg(player ? player->name : QStringLiteral("其他玩家")));
                }
            });

    connect(m_networkBridge, &NetworkGameBridge::tradeProposalReceived,
            this, &MainWindow::handleIncomingTradeProposal);
    connect(m_networkBridge, &NetworkGameBridge::tradeAcceptedReceived,
            this, &MainWindow::handleIncomingTradeAccepted);
    connect(m_networkBridge, &NetworkGameBridge::tradeRejectedReceived,
            this, &MainWindow::handleIncomingTradeRejected);
    connect(m_networkBridge, &NetworkGameBridge::tradeEndVoteReceived,
            this, &MainWindow::handleTradeEndVote);

    connect(m_controller, &GameController::tileChoiceRequested,
            this, &MainWindow::handleTileChoiceRequested);

    connect(m_controller, &GameController::buildChoiceRequested,
            this, &MainWindow::handleBuildChoiceRequested);

    connect(m_playerPanel, &PlayerPanel::playerClicked,
            this, [this](int playerId) {
                showPlayerInfoDialog(playerId);
            });

    connect(m_playerPanel, &PlayerPanel::tradeWithPlayerRequested,
            this, &MainWindow::openTradeDialogWithPlayer);

    connect(m_networkBridge, &NetworkGameBridge::playerDisplayNameReceived,
            this, &MainWindow::handleRemotePlayerDisplayName);
}

QString MainWindow::chooseBoardTheme(const QString& title)
{
    QDialog dialog(this);
    dialog.setWindowTitle(title);
    dialog.resize(760, 520);
    dialog.setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    dialog.setStyleSheet(QStringLiteral(
        "QDialog { background: #2f281f; }"
        "QLabel { color: #fce7b2; font-size: 16px; font-weight: bold; }"
        "QListWidget { background: #fff3d8; color: #3b2c21; border: 2px solid #bd8b49; border-radius: 10px; padding: 10px; }"
        "QListWidget::item { padding: 8px; border-radius: 8px; }"
        "QListWidget::item:selected { background: rgba(145, 93, 42, 80); }"
        "QPushButton { background: #6b3f2b; color: #fff1c4; border: 1px solid #b58a4a; border-radius: 6px; padding: 8px 18px; font-weight: bold; }"
        "QPushButton:hover { background: #815037; }"));

    auto* root = new QVBoxLayout(&dialog);
    auto* label = new QLabel(QStringLiteral("请选择本局使用的地图。加入房间的玩家会跟随房主地图，不能单独选择。"), &dialog);
    label->setWordWrap(true);
    root->addWidget(label);

    auto* list = new QListWidget(&dialog);
    list->setViewMode(QListView::IconMode);
    list->setResizeMode(QListView::Adjust);
    list->setMovement(QListView::Static);
    list->setIconSize(QSize(220, 164));
    list->setSpacing(12);

    const QList<BoardTheme> themes = availableBoardThemes();
    for (const BoardTheme& theme : themes) {
        auto* item = new QListWidgetItem(QIcon(theme.resourcePath), theme.displayName);
        item->setData(Qt::UserRole, theme.key);
        item->setTextAlignment(Qt::AlignCenter);
        item->setSizeHint(QSize(240, 205));
        list->addItem(item);
        if (theme.key == m_controller->boardThemeKey()) {
            list->setCurrentItem(item);
        }
    }
    if (!list->currentItem() && list->count() > 0) {
        list->setCurrentRow(0);
    }
    root->addWidget(list, 1);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->addStretch(1);
    auto* okButton = new QPushButton(QStringLiteral("使用此地图"), &dialog);
    buttonRow->addWidget(okButton);
    root->addLayout(buttonRow);

    connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(list, &QListWidget::itemDoubleClicked, &dialog, [&dialog](QListWidgetItem*) {
        dialog.accept();
    });

    if (dialog.exec() == QDialog::Accepted && list->currentItem()) {
        const QString selectedThemeKey = list->currentItem()->data(Qt::UserRole).toString();
        return selectedThemeKey.trimmed().isEmpty() ? defaultBoardThemeKey() : selectedThemeKey;
    }
    return defaultBoardThemeKey();
}


void MainWindow::appendChatMessage(const QString& sender,
                                   const QString& target,
                                   const QString& text,
                                   bool outgoing)
{
    if (!m_chatHistory) {
        return;
    }

    const QString time = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm"));
    const QString safeSender = sender.trimmed().isEmpty()
        ? (outgoing ? QStringLiteral("我") : QStringLiteral("其他玩家"))
        : sender.trimmed();
    const QString safeText = text.trimmed();
    if (safeText.isEmpty()) {
        return;
    }

    QString line;
    if (outgoing && !target.trimmed().isEmpty()) {
        line = QStringLiteral("[%1] %2 → %3：%4")
            .arg(time, safeSender, target.trimmed(), safeText);
    } else {
        line = QStringLiteral("[%1] %2：%3")
            .arg(time, safeSender, safeText);
    }

    const QString color = outgoing ? QStringLiteral("#8F312A") : QStringLiteral("#203A3F");
    m_chatHistory->append(QStringLiteral("<div style='color:%1; margin-bottom:4px;'>%2</div>")
                          .arg(color, line.toHtmlEscaped()));
    if (m_chatHistory->verticalScrollBar()) {
        m_chatHistory->verticalScrollBar()->setValue(m_chatHistory->verticalScrollBar()->maximum());
    }
}

void MainWindow::openChatMessageDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("交易阶段快捷聊天"));
    dialog.resize(840, 900);
    dialog.setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    dialog.setStyleSheet(QStringLiteral(
        "QDialog { background: #203A3F; color: #F4EADB; }"
        "QLabel { color: #F4EADB; font-family: 'Microsoft YaHei'; font-weight: 700; font-size: 18px; }"
        "QLabel#chatTitle { color: #E6CD8E; font-size: 28px; font-weight: 900; }"
        "QComboBox { background: #F4EADB; color: #2C332F; border: 2px solid #D8B56A; border-radius: 10px; padding: 8px; font-size: 16px; min-height: 32px; }"
        "QTextEdit { background: #F4EADB; color: #2C332F; border: 2px solid #D8B56A; border-radius: 10px; padding: 10px; font-size: 16px; }"
        "QPushButton { background: #8F312A; color: #FFF8EC; border: 2px solid #D8B56A; border-radius: 9px; padding: 9px 14px; font-weight: 800; font-size: 16px; }"
        "QPushButton:hover { background: #9B2F28; border-color: #E6CD8E; }"));

    auto* root = new QVBoxLayout(&dialog);
    root->setContentsMargins(22, 20, 22, 20);
    root->setSpacing(12);

    const Player* localPlayer = m_controller ? m_controller->playerById(m_controller->localPlayerId()) : nullptr;
    auto* title = new QLabel(QStringLiteral("%1 的交易喊话")
                                 .arg(localPlayer ? localPlayer->name : QStringLiteral("玩家")), &dialog);
    title->setObjectName(QStringLiteral("chatTitle"));
    title->setAlignment(Qt::AlignCenter);
    root->addWidget(title);

    auto* targetLabel = new QLabel(QStringLiteral("选择对谁说："), &dialog);
    root->addWidget(targetLabel);

    auto* targetCombo = new QComboBox(&dialog);
    targetCombo->addItem(QStringLiteral("所有玩家"), -1);
    if (m_controller) {
        const int localId = m_controller->localPlayerId();
        for (const Player& player : m_controller->players()) {
            if (player.active && player.id != localId) {
                targetCombo->addItem(player.name, player.id);
            }
        }
    }
    root->addWidget(targetCombo);

    auto* phraseLabel = new QLabel(QStringLiteral("顽皮快捷话术："), &dialog);
    root->addWidget(phraseLabel);

    auto* phraseGrid = new QGridLayout();
    phraseGrid->setHorizontalSpacing(10);
    phraseGrid->setVerticalSpacing(10);
    root->addLayout(phraseGrid);

    auto* input = new QTextEdit(&dialog);
    input->setPlaceholderText(QStringLiteral("可以直接输入，也可以点上面的快捷话术和 emoji。"));
    input->setMinimumHeight(140);

    auto appendToInput = [input](const QString& text) {
        if (!input) {
            return;
        }
        QString current = input->toPlainText();
        if (!current.isEmpty() && !current.endsWith(QLatin1Char(' ')) && !current.endsWith(QLatin1Char('\n'))) {
            current += QStringLiteral(" ");
        }
        current += text;
        input->setPlainText(current);
        QTextCursor cursor = input->textCursor();
        cursor.movePosition(QTextCursor::End);
        input->setTextCursor(cursor);
        input->setFocus();
    };

    const QStringList phrases = {
        QStringLiteral("快点吧，黄瓜菜都凉了 😂"),
        QStringLiteral("别犹豫了，这波血赚!"),
        QStringLiteral("我有一个大胆的想法。"),
        QStringLiteral("这地块我盯很久了。"),
        QStringLiteral("给个面子，合作共赢。"),
        QStringLiteral("这波不亏，信我一次。"),
        QStringLiteral("全场最佳谈判官上线。"),
        QStringLiteral("再磨叽，财神爷都下班了。"),
        QStringLiteral("听劝，真香预警。"),
        QStringLiteral("成交吧，别让我红温。"),
        QStringLiteral("暂时无法给你明确的答复，这个需要你自行衡量")
    };

    for (int i = 0; i < phrases.size(); ++i) {
        auto* button = new QPushButton(phrases.at(i), &dialog);
        button->setMinimumHeight(46);
        connect(button, &QPushButton::clicked, &dialog, [appendToInput, text = phrases.at(i)] {
            appendToInput(text);
        });
        phraseGrid->addWidget(button, i / 2, i % 2);
    }

    auto* emojiLabel = new QLabel(QStringLiteral("Emoji:"), &dialog);
    root->addWidget(emojiLabel);

    auto* emojiRow = new QHBoxLayout();
    emojiRow->setSpacing(12);
    const QStringList emojis = {
        QStringLiteral("😂"), QStringLiteral("🤝"), QStringLiteral("💰"), QStringLiteral("🔥"),
        QStringLiteral("😎"), QStringLiteral("😳"), QStringLiteral("🧧"), QStringLiteral("🏮"),
        QStringLiteral("👀"), QStringLiteral("✅")
    };
    for (const QString& emoji : emojis) {
        auto* button = new QPushButton(emoji, &dialog);
        button->setFixedSize(56, 50);
        connect(button, &QPushButton::clicked, &dialog, [appendToInput, emoji] {
            appendToInput(emoji);
        });
        emojiRow->addWidget(button);
    }
    emojiRow->addStretch(1);
    root->addLayout(emojiRow);

    root->addWidget(input, 1);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->addStretch(1);
    auto* cancelButton = new QPushButton(QStringLiteral("取消"), &dialog);
    auto* sendButton = new QPushButton(QStringLiteral("发送"), &dialog);
    buttonRow->addWidget(cancelButton);
    buttonRow->addWidget(sendButton);
    root->addLayout(buttonRow);

    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(sendButton, &QPushButton::clicked, &dialog, [this, &dialog, input, targetCombo] {
        const QString text = input ? input->toPlainText().trimmed() : QString();
        if (text.isEmpty()) {
            ToastMessage::showMessage(&dialog, QStringLiteral("先输入要发送的内容。"));
            return;
        }

        const QString targetName = targetCombo ? targetCombo->currentText() : QStringLiteral("所有玩家");
        if (m_networkBridge && m_networkBridge->isInRoom()) {
            m_networkBridge->sendChat(text, targetName);
        } else {
            appendChatMessage(QStringLiteral("我"), targetName, text, true);
        }
        dialog.accept();
    });


    dialog.exec();
}

void MainWindow::initializeBgm()
{
    if (m_bgmPlayer) {
        return;
    }

    m_bgmPlayer = new QMediaPlayer(this);
    m_bgmAudioOutput = new QAudioOutput(this);
    m_bgmPlayer->setAudioOutput(m_bgmAudioOutput);
    setBgmVolume(m_bgmVolume);

    connect(m_bgmPlayer, &QMediaPlayer::mediaStatusChanged,
            this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia && m_bgmPlayer) {
            m_bgmPlayer->play();
        }
    });
}

void MainWindow::updateBgmForCurrentRound()
{
    if (!m_controller) {
        return;
    }
    if (!m_bgmPlayer) {
        initializeBgm();
    }
    if (!m_bgmPlayer) {
        return;
    }

    const int round = std::max(1, m_controller->state().round);
    const int bgmIndex = (round - 1) % 3;
    const QStringList bgmSources = {
        QStringLiteral("qrc:/audio/bgm_round_1.mp3"),
        QStringLiteral("qrc:/audio/bgm_round_2.mp3"),
        QStringLiteral("qrc:/audio/bgm_round_3.mp3")
    };

    if (bgmIndex != m_currentBgmIndex) {
        m_currentBgmIndex = bgmIndex;
        m_bgmPlayer->setSource(QUrl(bgmSources.at(bgmIndex)));
    }

    if (m_bgmPlayer->playbackState() != QMediaPlayer::PlayingState) {
        m_bgmPlayer->play();
    }
}

void MainWindow::setBgmVolume(qreal volume)
{
    m_bgmVolume = std::clamp(volume, qreal(0.0), qreal(1.0));
    if (m_bgmAudioOutput) {
        m_bgmAudioOutput->setVolume(m_bgmVolume);
    }

    if (m_bgmVolumeButton) {
        const int percent = int(m_bgmVolume * 100.0 + 0.5);
        m_bgmVolumeButton->setText(QStringLiteral("BGM音量 %1%").arg(percent));
        m_bgmVolumeButton->setToolTip(QStringLiteral("当前 BGM 音量：%1%。点击调小 10%；到 0% 后再次点击恢复 45%。").arg(percent));
    }
}

void MainWindow::reduceBgmVolume()
{
    qreal nextVolume = m_bgmVolume <= 0.01 ? qreal(0.45) : (m_bgmVolume - qreal(0.10));
    if (nextVolume < 0.05) {
        nextVolume = 0.0;
    }
    setBgmVolume(nextVolume);

    if (statusBar()) {
        statusBar()->showMessage(QStringLiteral("BGM 音量已调为 %1%").arg(int(m_bgmVolume * 100.0 + 0.5)), 1800);
    }
}

void MainWindow::showStartupDialog()
{
    auto revealMainWindow = [this] {
        setWindowOpacity(1.0);
        show();
        raise();
        activateWindow();
    };

    auto startLocalGameNow = [this, revealMainWindow](bool chooseMap) {
        if (chooseMap) {
            m_controller->setBoardThemeKey(chooseBoardTheme(QStringLiteral("单机模式：选择地图")));
        }
        m_startupFinished = true;
        m_finalSettlementShown = false;
        m_tradeEndVotes.clear();
        m_startupNetworkHost = false;
        m_controller->setAutoplayNonLocalPlayers(true);
        revealMainWindow();
        m_controller->startLocalGame(4, 1);
        QTimer::singleShot(300, m_controller, &GameController::dealInitialTiles);
    };

    auto showRulesDialog = [this](QWidget* parentWidget) {
        QDialog rulesDialog(parentWidget);
        rulesDialog.setWindowTitle(QStringLiteral("游戏规则"));
        rulesDialog.resize(760, 680);
        rulesDialog.setWindowFlag(Qt::WindowContextHelpButtonHint, false);
        rulesDialog.setStyleSheet(QStringLiteral(
            "QDialog { background: #2f281f; }"
            "QLabel#rulesTitle { color: #ffe3a4; font-size: 22px; font-weight: bold; }"
            "QLabel#rulesBody { color: #3b2c21; background: #fff3d8; border: 2px solid #bd8b49; border-radius: 10px; padding: 16px; font-size: 15px; line-height: 145%; }"
            "QScrollArea { border: none; background: transparent; }"
            "QPushButton { background: #6b3f2b; color: #fff1c4; border: 1px solid #b58a4a; border-radius: 6px; padding: 8px 18px; }"));

        auto* layout = new QVBoxLayout(&rulesDialog);
        auto* title = new QLabel(QStringLiteral("唐人街：霓虹与黄金 · 规则概要"), &rulesDialog);
        title->setObjectName(QStringLiteral("rulesTitle"));
        layout->addWidget(title);

        auto* area = new QScrollArea(&rulesDialog);
        area->setWidgetResizable(true);
        auto* body = new QLabel(area);
        body->setObjectName(QStringLiteral("rulesBody"));
        body->setWordWrap(true);
        body->setTextFormat(Qt::RichText);
        body->setText(QStringLiteral(R"HTML(
<h3>目标</h3>
<p>游戏进行 6 轮，从 1965 年到 1970 年。第 6 轮收入结算后，现金最多的玩家获胜；若平手，地图上拥有店铺最多的玩家获胜。</p>
<h3>每轮流程</h3>
<ol>
<li><b>获得建筑卡牌：</b>每位玩家按人数和轮次获得地块/建筑编号卡，保留指定数量，多余弃掉。保留的编号对应地图上的地皮所有权。</li>
<li><b>抽取店铺卡片：</b>每位玩家按规则从袋中抽店铺卡，所有人同时翻开后进入交易。</li>
<li><b>谈判交易：</b>玩家可以自由交换金钱、地皮、店铺卡和已在地图上的店铺所有权；交易可以不等价，也可以组合多种资产。金钱金额对其他玩家保密。</li>
<li><b>放置店铺卡片：</b>按玩家顺序，把手中的店铺放到自己空地皮上，也可以选择暂不放。相同类型店铺只有上下左右相邻才会扩大规模；相邻但属于不同玩家时视为不同店铺。</li>
<li><b>获得收入：</b>每轮结束时，只统计地图上已建立并属于自己的店铺。手牌里的店铺不产生收入。</li>
<li><b>时间前进：</b>轮末时间标志前进 1 年，进入下一轮。</li>
</ol>
<h3>店铺规模与收入</h3>
<p>连锁规模按相同类型、同一玩家、上下左右相邻的店铺数量计算。达到该类型最大数量需求时，按“满编”收入结算；超过最大需求时，拆成一组满编店铺和剩余店铺分别计算。</p>
<table cellspacing="0" cellpadding="6" border="1">
<tr><th>每处规模</th><th>1</th><th>2</th><th>3</th><th>4</th><th>5</th><th>6</th></tr>
<tr><td>未达到最大需求</td><td>10,000</td><td>20,000</td><td>40,000</td><td>60,000</td><td>80,000</td><td>-</td></tr>
<tr><td>达到最大需求</td><td>-</td><td>-</td><td>50,000</td><td>80,000</td><td>110,000</td><td>140,000</td></tr>
</table>
<p>一旦店铺卡片放到地图上，就不能移动或移除；但店铺所有权在后续交易阶段仍然可以被交换。</p>
)HTML"));
        area->setWidget(body);
        layout->addWidget(area, 1);

        auto* closeButton = new QPushButton(QStringLiteral("关闭"), &rulesDialog);
        layout->addWidget(closeButton, 0, Qt::AlignRight);
        connect(closeButton, &QPushButton::clicked, &rulesDialog, &QDialog::accept);
        rulesDialog.exec();
    };

    QDialog cover(this);
    cover.setObjectName(QStringLiteral("coverDialog"));
    cover.setWindowTitle(QStringLiteral("唐人街：霓虹与黄金"));
    cover.resize(1200, 676);
    cover.setFixedSize(1200, 676);
    cover.setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    cover.setStyleSheet(QStringLiteral(
        "QDialog#coverDialog { border-image: url(:/images/ui/cover.png) 0 0 0 0 stretch stretch; background: #15120f; }"
        "QPushButton#coverMenuButton {"
        " background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #8e5a2a, stop:1 #5c341b);"
        " color: #f8ebc6;"
        " border: 2px solid #cda264;"
        " border-radius: 18px;"
        " font-size: 20px;"
        " font-weight: 700;"
        " padding: 10px 18px;"
        " text-align: center;"
        " }"
        "QPushButton#coverMenuButton:hover {"
        " background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #a86a31, stop:1 #6c3d20);"
        " border: 3px solid #f1d08c;"
        " }"
        "QPushButton#coverMenuButton:pressed {"
        " background: #5b331a;"
        " }"));

    auto* startButton = new QPushButton(QStringLiteral("开始游戏"), &cover);
    startButton->setObjectName(QStringLiteral("coverMenuButton"));
    startButton->setToolTip(QStringLiteral("开始游戏"));
    startButton->setGeometry(980, 448, 185, 58);

    auto* rulesButton = new QPushButton(QStringLiteral("游戏规则"), &cover);
    rulesButton->setObjectName(QStringLiteral("coverMenuButton"));
    rulesButton->setToolTip(QStringLiteral("游戏规则"));
    rulesButton->setGeometry(980, 520, 185, 58);

    auto* settingsButton = new QPushButton(QStringLiteral("设置"), &cover);
    settingsButton->setObjectName(QStringLiteral("coverMenuButton"));
    settingsButton->setToolTip(QStringLiteral("设置"));
    settingsButton->setGeometry(980, 592, 185, 58);

    connect(startButton, &QPushButton::clicked, &cover, &QDialog::accept);
    connect(rulesButton, &QPushButton::clicked, &cover, [&cover, showRulesDialog] {
        showRulesDialog(&cover);
    });

    connect(settingsButton, &QPushButton::clicked, &cover, [&cover] {
        QDialog settingsDialog(&cover);
        settingsDialog.setWindowTitle(QStringLiteral("设置"));
        settingsDialog.resize(420, 260);
        settingsDialog.setWindowFlag(Qt::WindowContextHelpButtonHint, false);
        settingsDialog.setStyleSheet(QStringLiteral(
            "QDialog { background: #2f281f; }"
            "QLabel { color: #fce7b2; font-size: 16px; }"
            "QPushButton { background: #6b3f2b; color: #fff1c4; border: 1px solid #b58a4a; border-radius: 6px; padding: 8px 18px; }"));
        auto* layout = new QVBoxLayout(&settingsDialog);
        auto* spacerText = new QLabel(QStringLiteral("设置项暂未添加。"), &settingsDialog);
        spacerText->setAlignment(Qt::AlignCenter);
        layout->addWidget(spacerText, 1);
        auto* closeButton = new QPushButton(QStringLiteral("关闭"), &settingsDialog);
        layout->addWidget(closeButton, 0, Qt::AlignRight);
        connect(closeButton, &QPushButton::clicked, &settingsDialog, &QDialog::accept);
        settingsDialog.exec();
    });

    if (cover.exec() != QDialog::Accepted) {
        startLocalGameNow(false);
        return;
    }

    QDialog modeDialog(this);
    modeDialog.setWindowTitle(QStringLiteral("选择游戏模式"));
    modeDialog.resize(480, 330);
    modeDialog.setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    modeDialog.setStyleSheet(QStringLiteral(
        "QDialog { background: #2f281f; }"
        "QLabel { color: #fce7b2; font-size: 16px; }"
        "QPushButton { background: #6b3f2b; color: #fff1c4; border: 1px solid #b58a4a; border-radius: 8px; padding: 12px 18px; font-size: 18px; font-weight: bold; }"
        "QPushButton:hover { background: #815037; }"));
    auto* modeRoot = new QVBoxLayout(&modeDialog);
    auto* modeTitle = new QLabel(QStringLiteral("请选择进入方式"), &modeDialog);
    modeTitle->setAlignment(Qt::AlignCenter);
    modeTitle->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 20px;"));
    modeRoot->addWidget(modeTitle);
    auto* createRoomButton = new QPushButton(QStringLiteral("创建房间"), &modeDialog);
    auto* joinRoomButton = new QPushButton(QStringLiteral("加入房间"), &modeDialog);
    auto* singlePlayerButton = new QPushButton(QStringLiteral("单机模式"), &modeDialog);
    modeRoot->addWidget(createRoomButton);
    modeRoot->addWidget(joinRoomButton);
    modeRoot->addWidget(singlePlayerButton);

    int selectedMode = 0; 
    connect(createRoomButton, &QPushButton::clicked, &modeDialog, [&] {
        selectedMode = 1;
        modeDialog.accept();
    });
    connect(joinRoomButton, &QPushButton::clicked, &modeDialog, [&] {
        selectedMode = 2;
        modeDialog.accept();
    });
    connect(singlePlayerButton, &QPushButton::clicked, &modeDialog, [&] {
        selectedMode = 3;
        modeDialog.accept();
    });

    if (modeDialog.exec() != QDialog::Accepted || selectedMode == 0) {
        startLocalGameNow(false);
        return;
    }

    if (selectedMode == 3) {
        startLocalGameNow(true);
        return;
    }

    const bool hostMode = selectedMode == 1;
    QDialog dialog(this);
    dialog.setWindowTitle(hostMode ? QStringLiteral("创建房间") : QStringLiteral("加入房间"));
    dialog.resize(hostMode ? 460 : 520, hostMode ? 300 : 360);
    dialog.setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    dialog.setStyleSheet(QStringLiteral(
        "QDialog { background: #2f281f; }"
        "QLabel { color: #fce7b2; }"
        "QLineEdit, QSpinBox { background: #fbf3dc; color: #3d3329; border: 1px solid #9a8465; border-radius: 5px; padding: 4px; }"
        "QPushButton { background: #6b3f2b; color: #fff1c4; border: 1px solid #b58a4a; border-radius: 6px; padding: 7px 12px; }"));

    auto* root = new QVBoxLayout(&dialog);
    auto* title = new QLabel(hostMode
        ? QStringLiteral("创建房间会在本机启动服务器，并自动进入该房间。本模式不启用人机，四个玩家编号都需要由真人客户端分别绑定。请让其他玩家填写你的局域网 IPv4 地址、端口和房间号。")
        : QStringLiteral("加入房间需要填写房主的服务器地址、端口和房间号。进入后绑定自己的玩家编号，等待房主开始游戏；联网模式不启用人机。"), &dialog);
    title->setWordWrap(true);
    title->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 15px;"));
    root->addWidget(title);

    auto* form = new QFormLayout();
    auto* playerId = new QSpinBox(&dialog);
    playerId->setRange(1, 4);
    playerId->setValue(1);
    auto* host = new QLineEdit(QStringLiteral("127.0.0.1"), &dialog);
    host->setEnabled(!hostMode);
    auto* port = new QSpinBox(&dialog);
    port->setRange(1, 65535);
    port->setValue(45454);
    auto* room = new QLineEdit(QStringLiteral("default"), &dialog);
    auto* name = new QLineEdit(QStringLiteral("玩家1"), &dialog);

    form->addRow(QStringLiteral("绑定玩家编号："), playerId);
    if (!hostMode) {
        form->addRow(QStringLiteral("服务器地址："), host);
    }
    form->addRow(QStringLiteral("端口："), port);
    form->addRow(QStringLiteral("房间号："), room);
    form->addRow(QStringLiteral("显示名称："), name);
    root->addLayout(form);

    connect(playerId, qOverload<int>(&QSpinBox::valueChanged), &dialog, [name](int value) {
        name->setText(QStringLiteral("玩家%1").arg(value));
    });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(hostMode ? QStringLiteral("创建并进入") : QStringLiteral("加入房间"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消并单机开始"));
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        startLocalGameNow(true);
        return;
    }

    m_startupBoundPlayerId = playerId->value();
    m_startupPlayerName = name->text().trimmed().isEmpty()
        ? QStringLiteral("玩家%1").arg(m_startupBoundPlayerId)
        : name->text().trimmed();
    rememberPlayerDisplayName(m_startupBoundPlayerId, m_startupPlayerName);
    m_networkBridge->setBoundPlayerId(m_startupBoundPlayerId);
    m_controller->setAutoplayNonLocalPlayers(false);
    m_startupNetworkHost = hostMode;

    if (m_startupNetworkHost) {
        m_controller->setBoardThemeKey(chooseBoardTheme(QStringLiteral("创建房间：选择地图")));
    }

    if (m_startupNetworkHost) {
        if (m_localServer && m_localServer->listen(static_cast<quint16>(port->value()))) {
            m_localServerRunning = true;
            if (m_logPanel) {
                m_logPanel->appendMessage(QStringLiteral("房主服务器已启动，端口 %1。其他玩家连接房主电脑的 IPv4 地址。").arg(port->value()));
            }
        } else {
            QMessageBox::warning(this, QStringLiteral("服务器启动失败"),
                                 QStringLiteral("端口可能被占用或被防火墙拦截。请重新进入，或改用加入房间/单机模式。"));
            QTimer::singleShot(0, this, &MainWindow::showStartupDialog);
            return;
        }
    }

    m_startupFinished = true;
    revealMainWindow();
    m_topBar->setText(QStringLiteral("正在进入房间 %1，绑定玩家 %2...").arg(room->text()).arg(m_startupBoundPlayerId));
    m_networkBridge->connectAndJoin(hostMode ? QStringLiteral("127.0.0.1") : host->text(),
                                    static_cast<quint16>(port->value()),
                                    m_startupPlayerName,
                                    room->text(),
                                    m_startupBoundPlayerId);
}

void MainWindow::handleNetworkRoomJoined(const QString& roomId)
{
    if (m_startupNetworkHost) {
        if (m_logPanel) {
            m_logPanel->appendMessage(QStringLiteral("房主已进入房间 %1。请等其他玩家也进入同一房间后，再开始游戏。").arg(roomId));
        }
        QMessageBox readyBox(this);
        readyBox.setWindowTitle(QStringLiteral("房主准备开始"));
        readyBox.setText(QStringLiteral("你已经作为房主进入房间 %1。\n\n请确认另外三位真人玩家已经用同一个房间号加入，并且分别绑定玩家 2、3、4 后，再点击“开始游戏”。点击后才会进入抽地块阶段。")
                         .arg(roomId));
        readyBox.setStandardButtons(QMessageBox::Ok);
        readyBox.button(QMessageBox::Ok)->setText(QStringLiteral("开始游戏"));
        readyBox.exec();

        if (m_logPanel) {
            m_logPanel->appendMessage(QStringLiteral("房主开始创建棋局并同步初始状态。"));
        }

        m_finalSettlementShown = false;
        m_tradeEndVotes.clear();
        m_controller->setAutoplayNonLocalPlayers(false);
        m_controller->startLocalGame(4, m_startupBoundPlayerId);
        applyKnownPlayerDisplayNames();
        broadcastLocalPlayerDisplayName();
        QTimer::singleShot(200, this, [this] {
            applyKnownPlayerDisplayNames();
            m_networkBridge->broadcastStateNow(QStringLiteral("hostInit"));
            m_controller->dealInitialTiles();
            QTimer::singleShot(200, this, [this] {
                m_networkBridge->broadcastStateNow(QStringLiteral("hostStartDrawTiles"));
            });
        });
    } else {
        m_topBar->setText(QStringLiteral("已进入房间 %1，绑定玩家 %2，等待房主同步棋局...")
                          .arg(roomId)
                          .arg(m_startupBoundPlayerId));
        if (m_logPanel) {
            m_logPanel->appendMessage(QStringLiteral("已进入房间 %1。普通玩家不会自行开始抽地块，等待房主同步状态。")
                                      .arg(roomId));
        }
        broadcastLocalPlayerDisplayName();
    }
}

void MainWindow::rememberPlayerDisplayName(int playerId, const QString& displayName)
{
    const QString cleaned = displayName.trimmed();
    if (playerId <= 0 || cleaned.isEmpty()) {
        return;
    }
    m_knownPlayerDisplayNames[playerId] = cleaned.left(16);
    if (m_controller) {
        m_controller->setPlayerDisplayName(playerId, m_knownPlayerDisplayNames.value(playerId));
    }
}

void MainWindow::applyKnownPlayerDisplayNames()
{
    if (!m_controller || m_applyingKnownDisplayNames || m_knownPlayerDisplayNames.isEmpty()) {
        return;
    }

    m_applyingKnownDisplayNames = true;
    for (auto it = m_knownPlayerDisplayNames.cbegin(); it != m_knownPlayerDisplayNames.cend(); ++it) {
        m_controller->setPlayerDisplayName(it.key(), it.value());
    }
    m_applyingKnownDisplayNames = false;
}

void MainWindow::broadcastLocalPlayerDisplayName()
{
    if (!m_networkBridge || !m_networkBridge->isInRoom()) {
        return;
    }

    const int playerId = m_startupBoundPlayerId > 0 ? m_startupBoundPlayerId : m_networkBridge->boundPlayerId();
    QString displayName = m_knownPlayerDisplayNames.value(playerId);
    if (displayName.trimmed().isEmpty()) {
        displayName = m_networkBridge->playerName().trimmed();
    }
    if (displayName.trimmed().isEmpty()) {
        displayName = QStringLiteral("玩家%1").arg(playerId);
    }

    rememberPlayerDisplayName(playerId, displayName);

    QJsonObject data;
    data[QStringLiteral("playerId")] = playerId;
    data[QStringLiteral("displayName")] = displayName.left(16);
    m_networkBridge->sendGameAction(QStringLiteral("playerDisplayName"), data);
}

void MainWindow::handleRemotePlayerDisplayName(int playerId, const QString& displayName)
{
    rememberPlayerDisplayName(playerId, displayName);
    if (m_logPanel && playerId > 0 && !displayName.trimmed().isEmpty()) {
        m_logPanel->appendMessage(QStringLiteral("玩家%1 的显示名称已同步为：%2")
                                  .arg(playerId)
                                  .arg(displayName.trimmed().left(16)));
    }
}

void MainWindow::refreshAll()
{
    resetTradeEndVotesIfNeeded();
    applyKnownPlayerDisplayNames();

    const GameState& state = m_controller->state();
    const Player* local = m_controller->playerById(m_controller->localPlayerId());

    if (state.phase != GamePhase::DrawTile && m_activeTileChoicePlayerId > 0) {
        m_activeTileChoicePlayerId = -1;
        m_activeTileChoiceChooseCount = 0;
        m_activeTileChoiceCandidates.clear();
        m_activeTileChoiceSelected.clear();
    }
    if (state.phase != GamePhase::Build && m_activeBuildPlayerId > 0) {
        m_activeBuildPlayerId = -1;
    }

    if (state.phase == GamePhase::Trade) {
        for (const Player& player : state.players) {
            if (player.tradeFinishedThisRound) {
                m_tradeEndVotes.insert(player.id);
            }
        }
    }

    const QString newTopText = QStringLiteral("第 %1 轮 ｜ %2 ｜ 本机玩家：%3 ｜ 我的资金：%4")
        .arg(state.round)
        .arg(phaseName(state.phase))
        .arg(local ? local->name : QStringLiteral("无"))
        .arg(local ? QString::number(local->money) : QStringLiteral("无"));
    if (newTopText != m_lastTopBarText) {
        m_lastTopBarText = newTopText;
        m_topBar->setText(newTopText);
    }

    m_playerPanel->setCurrentPlayer(m_controller->localPlayerId());
    m_playerPanel->setPlayers(m_controller->players());

    m_actionPanel->setPhase(m_controller->phase());
    updateTradeEndUi();

    if (state.selectedTileId > 0) {
        m_tileInfoPanel->showTile(state.selectedTileId);
    } else {
        m_tileInfoPanel->clear();
    }

    if (!m_finalSettlementShown && m_controller->isFinalSettlementReady() && !state.players.isEmpty()) {
        QTimer::singleShot(200, this, &MainWindow::showFinalSettlementDialog);
    }
}

void MainWindow::openLogDialog()
{
    if (!m_logDialog) {
        return;
    }

    m_logDialog->show();
    m_logDialog->raise();
    m_logDialog->activateWindow();
}

void MainWindow::showPhaseTransition(GamePhase phase)
{
    if (!m_phaseBanner) {
        return;
    }

    m_phaseBanner->setText(phaseName(phase));
    m_phaseBanner->show();
    if (auto* effect = qobject_cast<QGraphicsOpacityEffect*>(m_phaseBanner->graphicsEffect())) {
        effect->setOpacity(1.0);
    }
}

void MainWindow::resetMapInteractionState()
{
    m_activeTileChoicePlayerId = -1;
    m_activeTileChoiceChooseCount = 0;
    m_activeTileChoiceCandidates.clear();
    m_activeTileChoiceSelected.clear();
    m_activeBuildPlayerId = -1;
}

void MainWindow::handleBoardTileClicked(int tileId)
{
    const GamePhase phase = m_controller->phase();

    if (phase == GamePhase::DrawTile) {
        if (m_activeTileChoicePlayerId <= 0 || !m_activeTileChoiceCandidates.contains(tileId)) {
            ToastMessage::showMessage(this, QStringLiteral("请点击正在闪烁的候选地块。"));
            return;
        }

        if (m_activeTileChoiceSelected.contains(tileId)) {
            m_activeTileChoiceSelected.removeAll(tileId);
        } else {
            if (m_activeTileChoiceSelected.size() >= m_activeTileChoiceChooseCount) {
                ToastMessage::showMessage(this, QStringLiteral("最多只能选择 %1 个地块。再次点击已选地块可取消。")
                                          .arg(m_activeTileChoiceChooseCount));
                return;
            }
            m_activeTileChoiceSelected.append(tileId);
        }

        m_boardView->setChoiceSelectedTiles(m_activeTileChoiceSelected);
        updateTradeEndUi();
        return;
    }

    if (phase == GamePhase::Build) {
        if (m_activeBuildPlayerId <= 0) {
            ToastMessage::showMessage(this, QStringLiteral("请等待系统轮到本玩家建设。"));
            return;
        }

        const QList<int> buildable = m_controller->buildableTilesForPlayer(m_activeBuildPlayerId);
        if (!buildable.contains(tileId)) {
            ToastMessage::showMessage(this, QStringLiteral("地块 %1 当前不能建设。请点击闪烁提示的可建设空地块。").arg(tileId));
            return;
        }

        const Player* player = m_controller->playerById(m_activeBuildPlayerId);
        if (!player) {
            return;
        }

        QList<QPair<ShopType, int>> choices;
        for (ShopType type : allShopTypes()) {
            const int count = player->shopCards.value(type, 0);
            if (count > 0) {
                choices.append(qMakePair(type, count));
            }
        }

        if (choices.isEmpty()) {
            ToastMessage::showMessage(this, QStringLiteral("%1 没有可建设的店铺牌。").arg(player->name));
            return;
        }

        QDialog dialog(this);
        dialog.setWindowTitle(QStringLiteral("选择店铺"));
        dialog.setStyleSheet(QStringLiteral(
            "QDialog { background: #2f281f; }"
            "QLabel { color: #fce7b2; }"
            "QComboBox { background: #fff8e5; color: #3d3329; border: 1px solid #9a8465; border-radius: 4px; padding: 4px; }"
            "QPushButton { background: #6b3f2b; color: #fff1c4; border: 1px solid #b58a4a; border-radius: 6px; padding: 7px 12px; }"));
        auto* layout = new QVBoxLayout(&dialog);
        auto* tip = new QLabel(QStringLiteral("在地块 %1 上建设商铺。切换不同商铺时会先预览图片，方便你与相邻地块对照。").arg(tileId), &dialog);
        tip->setWordWrap(true);
        layout->addWidget(tip);

        auto* combo = new QComboBox(&dialog);
        for (const auto& entry : choices) {
            combo->addItem(QStringLiteral("%1 ×%2").arg(shopTypeName(entry.first)).arg(entry.second), static_cast<int>(entry.first));
        }
        layout->addWidget(combo);

        auto* preview = new QLabel(&dialog);
        preview->setFixedSize(150, 170);
        preview->setAlignment(Qt::AlignCenter);
        preview->setStyleSheet(QStringLiteral("background: #fbf3dc; color: #3d3329; border: 1px solid #9a8465; border-radius: 8px; padding: 6px;"));
        layout->addWidget(preview, 0, Qt::AlignHCenter);

        auto* previewText = new QLabel(&dialog);
        previewText->setWordWrap(true);
        layout->addWidget(previewText);

        auto updatePreview = [combo, preview, previewText] {
            const ShopType type = static_cast<ShopType>(combo->currentData().toInt());
            const QString key = QStringLiteral("shops/%1.png").arg(shopTypeKey(type));
            const QPixmap pix = AssetManager::instance().pixmap(key);
            preview->setPixmap(pix.scaled(preview->size() - QSize(12, 12), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            previewText->setText(QStringLiteral("预览：%1").arg(shopTypeName(type)));
        };
        updatePreview();
        connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), &dialog, [updatePreview](int) {
            updatePreview();
        });

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定建设"));
        buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout->addWidget(buttons);

        if (dialog.exec() != QDialog::Accepted) {
            return;
        }

        const ShopType selectedType = static_cast<ShopType>(combo->currentData().toInt());
        QString error;
        if (!m_controller->buildShop(m_activeBuildPlayerId, selectedType, QList<int>{tileId}, &error)) {
            ToastMessage::showMessage(this, error);
            return;
        }

        ToastMessage::showMessage(this, QStringLiteral("建设完成。可以继续点击其他闪烁边框地块，或点击右侧“完成/跳过建设”。"));
        m_boardView->clearTileEffects();
        m_boardView->showBuildableTiles(m_controller->buildableTilesForPlayer(m_activeBuildPlayerId));
        updateTradeEndUi();

        if (m_networkBridge && m_networkBridge->isInRoom()) {
            m_networkBridge->broadcastStateNow(QStringLiteral("buildAppliedByMapClick"));
        }
        return;
    }

    m_controller->selectTile(tileId);
}

void MainWindow::handleTileChoiceRequested(int playerId, QList<int> candidateTileIds, int chooseCount)
{
    if (m_networkBridge && m_networkBridge->isInRoom() && playerId != m_controller->localPlayerId()) {
        const Player* player = m_controller->playerById(playerId);
        m_activeTileChoicePlayerId = -1;
        m_activeTileChoiceCandidates.clear();
        m_activeTileChoiceSelected.clear();
        m_boardView->clearTileEffects();
        updateTradeEndUi();
        if (m_logPanel) {
            m_logPanel->appendMessage(QStringLiteral("当前轮到 %1 抽地块，本客户端只能等待，且不会提前看到前面玩家已保留的地块。")
                                      .arg(player ? player->name : QStringLiteral("其他玩家")));
        }
        return;
    }

    const Player* player = m_controller->playerById(playerId);
    m_activeTileChoicePlayerId = playerId;
    m_activeTileChoiceChooseCount = chooseCount;
    m_activeTileChoiceCandidates = candidateTileIds;
    m_activeTileChoiceSelected.clear();
    m_activeBuildPlayerId = -1;

    m_boardView->clearTileEffects();
    m_boardView->showChoiceCandidateTiles(candidateTileIds);
    m_boardView->setChoiceSelectedTiles(m_activeTileChoiceSelected);

    const QString message = QStringLiteral("%1 抽地块：请直接在地图上点击闪烁的候选地块，选择 %2 个后点击右侧确认。")
                                .arg(player ? player->name : QStringLiteral("玩家"))
                                .arg(chooseCount);
    ToastMessage::showMessage(this, message);
    if (m_logPanel) {
        m_logPanel->appendMessage(message);
    }
    updateTradeEndUi();
}

void MainWindow::confirmTileChoicesFromMap()
{
    if (m_controller->phase() != GamePhase::DrawTile || m_activeTileChoicePlayerId <= 0) {
        ToastMessage::showMessage(this, QStringLiteral("当前没有需要确认的地块选择。"));
        return;
    }

    if (m_activeTileChoiceSelected.size() != m_activeTileChoiceChooseCount) {
        ToastMessage::showMessage(this, QStringLiteral("请先在地图上选择正好 %1 个闪烁地块。")
                                  .arg(m_activeTileChoiceChooseCount));
        return;
    }

    const int playerId = m_activeTileChoicePlayerId;
    const QList<int> selected = m_activeTileChoiceSelected;
    m_activeTileChoicePlayerId = -1;
    m_activeTileChoiceChooseCount = 0;
    m_activeTileChoiceCandidates.clear();
    m_activeTileChoiceSelected.clear();
    m_boardView->clearTileEffects();
    updateTradeEndUi();

    m_controller->submitTileChoices(playerId, selected);

    if (m_networkBridge && m_networkBridge->isInRoom()) {
        m_networkBridge->broadcastStateNow(QStringLiteral("tileChoiceSubmittedByMap"));
    }
}

void MainWindow::openTradeDialog()
{
    const int localPlayerId = m_controller->localPlayerId();
    if (m_tradeEndVotes.contains(localPlayerId)) {
        ToastMessage::showMessage(this, QStringLiteral("你已经选择结束交易，不能再主动发起交易。"));
        return;
    }

    int targetPlayerId = -1;
    const int selectedTileId = m_controller->selectedTileId();
    const Tile* selectedTile = m_controller->tileById(selectedTileId);
    if (selectedTile && selectedTile->ownerId > 0 && selectedTile->ownerId != localPlayerId) {
        targetPlayerId = selectedTile->ownerId;
    }

    if (targetPlayerId <= 0) {
        QStringList choices;
        QMap<QString, int> ids;
        for (const Player& player : m_controller->players()) {
            if (player.id != localPlayerId && player.active && !player.tradeFinishedThisRound) {
                const QString text = QStringLiteral("%1（玩家%2）").arg(player.name).arg(player.id);
                choices << text;
                ids[text] = player.id;
            }
        }
        if (choices.isEmpty()) {
            ToastMessage::showMessage(this, QStringLiteral("没有可交易对象：已结束交易的玩家不能再参与交易。"));
            return;
        }
        bool ok = false;
        const QString choice = QInputDialog::getItem(this,
                                                     QStringLiteral("选择交易对象"),
                                                     QStringLiteral("请选择要发起交易的玩家："),
                                                     choices,
                                                     0,
                                                     false,
                                                     &ok);
        if (!ok || choice.isEmpty()) {
            return;
        }
        targetPlayerId = ids.value(choice, -1);
    }

    openTradeDialogWithPlayer(targetPlayerId);
}

void MainWindow::openTradeDialogWithPlayer(int targetPlayerId)
{
    const int localPlayerId = m_controller->localPlayerId();

    if (m_controller->phase() != GamePhase::Trade) {
        ToastMessage::showMessage(this, QStringLiteral("只有交易阶段可以交易"));
        return;
    }

    const Player* localPlayer = m_controller->playerById(localPlayerId);
    const Player* targetPlayer = m_controller->playerById(targetPlayerId);

    if (m_tradeEndVotes.contains(localPlayerId) || (localPlayer && localPlayer->tradeFinishedThisRound)) {
        ToastMessage::showMessage(this, QStringLiteral("你已经选择结束交易，不能再主动发起交易。"));
        return;
    }

    if (targetPlayer && targetPlayer->tradeFinishedThisRound) {
        ToastMessage::showMessage(this, QStringLiteral("%1 已经结束交易，已切换为查看其资产信息。").arg(targetPlayer->name));
        showPlayerInfoDialog(targetPlayerId);
        return;
    }

    if (targetPlayerId <= 0 || targetPlayerId == localPlayerId || !targetPlayer || !targetPlayer->active) {
        ToastMessage::showMessage(this, QStringLiteral("请选择仍在交易阶段中的其他玩家进行交易"));
        return;
    }

    TradeDialog dialog(this);
    dialog.setController(m_controller);
    dialog.setPlayers(localPlayerId, targetPlayerId);

    connect(&dialog, &TradeDialog::tradeConfirmed, this, [this](const TradeProposal& proposal) {
        QString reason;
        if (!m_controller->validateTrade(proposal, &reason)) {
            ToastMessage::showMessage(this, reason);
            return;
        }

        if (m_networkBridge && m_networkBridge->isInRoom()) {
            sendTradeProposalToNetwork(proposal);
            return;
        }

        TradeProposal pendingProposal = proposal;
        const Player* initialLeft = m_controller->playerById(pendingProposal.leftPlayerId);
        QString proposerName = initialLeft ? initialLeft->name : QStringLiteral("本机玩家");

        while (true) {
            TradeProposal counterProposal;
            const TradeDecision decision = askTradeApproval(pendingProposal, proposerName, &counterProposal);
            if (decision == TradeDecision::Rejected) {
                const Player* right = m_controller->playerById(pendingProposal.rightPlayerId);
                const QString name = right ? right->name : QStringLiteral("对方");
                ToastMessage::showMessage(this, QStringLiteral("%1 已拒绝交易，交易未成立。").arg(name));
                if (m_logPanel) {
                    m_logPanel->appendMessage(QStringLiteral("%1 拒绝了交易，交易未成立：\n%2")
                                              .arg(name)
                                              .arg(proposalSummary(m_controller, pendingProposal)));
                }
                return;
            }

            if (decision == TradeDecision::CounterOffer) {
                pendingProposal = counterProposal;
                const Player* newLeft = m_controller->playerById(pendingProposal.leftPlayerId);
                proposerName = newLeft ? newLeft->name : QStringLiteral("对方");
                if (m_logPanel) {
                    m_logPanel->appendMessage(QStringLiteral("收到新的讨价还价条件，改由 %1 发起新报价：\n%2")
                                              .arg(proposerName)
                                              .arg(proposalSummary(m_controller, pendingProposal)));
                }
                continue;
            }

            QString error;
            if (!m_controller->applyTrade(pendingProposal, &error)) {
                ToastMessage::showMessage(this, error);
            } else {
                ToastMessage::showMessage(this, QStringLiteral("对方已同意，交易完成。"));
                if (m_logPanel) {
                    m_logPanel->appendMessage(QStringLiteral("对方已同意交易，交易成立：\n%1")
                                              .arg(proposalSummary(m_controller, pendingProposal)));
                }
            }
            return;
        }
    });

    dialog.exec();
}

void MainWindow::sendTradeProposalToNetwork(const TradeProposal& proposal)
{
    QString reason;
    if (!m_controller->validateTrade(proposal, &reason)) {
        ToastMessage::showMessage(this, reason);
        return;
    }

    if (!m_networkBridge || !m_networkBridge->isInRoom()) {
        ToastMessage::showMessage(this, QStringLiteral("尚未联网，无法发送交易请求。"));
        return;
    }

    QJsonObject data;
    const QString proposalId = makeProposalId(proposal.leftPlayerId, proposal.rightPlayerId);
    data[QStringLiteral("proposalId")] = proposalId;
    data[QStringLiteral("proposal")] = proposalToJson(proposal);
    data[QStringLiteral("targetPlayerId")] = proposal.rightPlayerId;
    data[QStringLiteral("fromPlayerId")] = proposal.leftPlayerId;

    const Player* right = m_controller->playerById(proposal.rightPlayerId);
    if (m_logPanel) {
        m_logPanel->appendMessage(QStringLiteral("已向 %1 发起交易，等待对方同意/拒绝/讨价还价：\n%2")
                                  .arg(right ? right->name : QStringLiteral("对方"))
                                  .arg(proposalSummary(m_controller, proposal)));
    }
    ToastMessage::showMessage(this, QStringLiteral("交易请求已发送，等待对方确认。"));
    m_networkBridge->sendGameAction(QStringLiteral("tradeProposal"), data);
}

void MainWindow::sendCounterOfferToNetwork(const TradeProposal& proposal, const QString& basedOnProposalId)
{
    if (!m_networkBridge || !m_networkBridge->isInRoom()) {
        return;
    }

    QJsonObject data;
    data[QStringLiteral("proposalId")] = makeProposalId(proposal.leftPlayerId, proposal.rightPlayerId);
    if (!basedOnProposalId.isEmpty()) {
        data[QStringLiteral("counterToProposalId")] = basedOnProposalId;
    }
    data[QStringLiteral("isCounterOffer")] = true;
    data[QStringLiteral("proposal")] = proposalToJson(proposal);
    data[QStringLiteral("targetPlayerId")] = proposal.rightPlayerId;
    data[QStringLiteral("fromPlayerId")] = proposal.leftPlayerId;
    m_networkBridge->sendGameAction(QStringLiteral("tradeProposal"), data);

    const Player* right = m_controller->playerById(proposal.rightPlayerId);
    if (m_logPanel) {
        m_logPanel->appendMessage(QStringLiteral("已向 %1 发送讨价还价的新条件：\n%2")
                                  .arg(right ? right->name : QStringLiteral("对方"))
                                  .arg(proposalSummary(m_controller, proposal)));
    }
    ToastMessage::showMessage(this, QStringLiteral("新的交易条件已发送给对方。"));
}

void MainWindow::sendTradeRejectedToNetwork(const QString& proposalId, const QString& reason)
{
    if (!m_networkBridge || !m_networkBridge->isInRoom()) {
        return;
    }

    QJsonObject data;
    data[QStringLiteral("proposalId")] = proposalId;
    data[QStringLiteral("reason")] = reason.isEmpty() ? QStringLiteral("对方拒绝交易") : reason;
    data[QStringLiteral("fromPlayerId")] = m_controller->localPlayerId();
    m_networkBridge->sendGameAction(QStringLiteral("tradeRejected"), data);
}

MainWindow::TradeDecision MainWindow::askTradeApproval(const TradeProposal& proposal,
                                                  const QString& fromName,
                                                  TradeProposal* counterProposal)
{
    const Player* left = m_controller ? m_controller->playerById(proposal.leftPlayerId) : nullptr;
    const Player* right = m_controller ? m_controller->playerById(proposal.rightPlayerId) : nullptr;
    const QString leftName = left ? left->name : (fromName.isEmpty() ? QStringLiteral("发起方") : fromName);
    const QString rightName = right ? right->name : QStringLiteral("接收方");

    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("tradeApprovalDialog"));
    dialog.setWindowTitle(QStringLiteral("交易确认"));
    dialog.resize(780, 620);
    dialog.setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    dialog.setStyleSheet(QStringLiteral(
        "QDialog#tradeApprovalDialog { background: #2a1e15; border: 4px solid #c79b5a; border-radius: 20px; }"
        "QLabel { color: #fce7b2; }"
        "QLabel#approvalTitle { color: #ffe3a4; font-size: 24px; font-weight: 900; }"
        "QLabel#approvalSubtitle { color: #fff1c7; font-size: 15px; }"
        "QGroupBox { color: #ffe7b7; font-size: 15px; font-weight: 900; border: 3px solid #b88750; border-radius: 16px; margin-top: 14px; padding: 10px; background: rgba(255, 226, 160, 22); }"
        "QGroupBox::title { subcontrol-origin: margin; left: 16px; padding: 0 8px; }"
        "QListWidget { background: rgba(255, 244, 220, 240); color: #372719; border: 2px solid #b98a55; border-radius: 10px; padding: 6px; font-size: 14px; }"
        "QListWidget::item { padding: 8px 6px; border-bottom: 1px solid rgba(128, 91, 48, 55); }"
        "QPushButton { background: #6b3f2b; color: #fff1c4; border: 2px solid #bd9258; border-radius: 10px; padding: 10px 18px; font-size: 15px; font-weight: 900; }"
        "QPushButton:hover { background: #845036; border-color: #f1d08c; }"
        "QPushButton#acceptButton { background: #70552a; border-color: #e0bd72; }"
        "QPushButton#rejectButton { background: #63342d; }"
    ));

    auto* root = new QVBoxLayout(&dialog);
    root->setContentsMargins(18, 18, 18, 18);
    root->setSpacing(12);

    auto* title = new QLabel(QStringLiteral("收到交易请求"), &dialog);
    title->setObjectName(QStringLiteral("approvalTitle"));
    title->setAlignment(Qt::AlignCenter);
    root->addWidget(title);

    auto* subtitle = new QLabel(QStringLiteral("%1 向 %2 发起交易。请核对双方给出的资产和金额，确认后交易才会生效。")
                                    .arg(leftName, rightName), &dialog);
    subtitle->setObjectName(QStringLiteral("approvalSubtitle"));
    subtitle->setWordWrap(true);
    subtitle->setAlignment(Qt::AlignCenter);
    root->addWidget(subtitle);

    auto createOfferBox = [&dialog](const QString& titleText, const QString& playerName,
                                    const QList<TradeAsset>& assets, int money) {
        auto* box = new QGroupBox(titleText, &dialog);
        auto* layout = new QVBoxLayout(box);
        layout->setContentsMargins(10, 12, 10, 10);
        layout->setSpacing(8);

        auto* nameLabel = new QLabel(QStringLiteral("%1 给出金额：¥ %2")
                                         .arg(playerName)
                                         .arg(money), box);
        nameLabel->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: 900; color: #ffe9b2;"));
        layout->addWidget(nameLabel);

        auto* list = new QListWidget(box);
        list->setMinimumHeight(240);
        if (assets.isEmpty()) {
            list->addItem(QStringLiteral("无资产"));
        } else {
            for (const TradeAsset& asset : assets) {
                list->addItem(tradeAssetName(asset));
            }
        }
        layout->addWidget(list, 1);
        return box;
    };

    auto* columns = new QHBoxLayout();
    columns->setSpacing(14);
    columns->addWidget(createOfferBox(QStringLiteral("左方交易内容"), leftName, proposal.leftAssets, proposal.leftMoney), 1);
    columns->addWidget(createOfferBox(QStringLiteral("右方交易内容"), rightName, proposal.rightAssets, proposal.rightMoney), 1);
    root->addLayout(columns, 1);

    QString reason;
    const bool ok = m_controller && m_controller->validateTrade(proposal, &reason);
    auto* validationLabel = new QLabel(ok
        ? QStringLiteral("校验结果：可以成交。双方确认后，地块及其已建店铺将按交易内容同步转移。")
        : QStringLiteral("校验结果：当前不能成交。原因：%1").arg(reason), &dialog);
    validationLabel->setWordWrap(true);
    validationLabel->setStyleSheet(ok
        ? QStringLiteral("background: rgba(125, 156, 84, 40); border: 2px solid #91b46a; border-radius: 10px; padding: 10px; color: #f7ffd4; font-size: 14px;")
        : QStringLiteral("background: rgba(156, 77, 72, 45); border: 2px solid #b9665b; border-radius: 10px; padding: 10px; color: #ffe0d8; font-size: 14px;"));
    root->addWidget(validationLabel);

    auto* buttonRow = new QHBoxLayout();
    auto* rejectButton = new QPushButton(QStringLiteral("拒绝交易"), &dialog);
    rejectButton->setObjectName(QStringLiteral("rejectButton"));
    auto* counterButton = new QPushButton(QStringLiteral("讨价还价"), &dialog);
    auto* acceptButton = new QPushButton(QStringLiteral("我同意此交易"), &dialog);
    acceptButton->setObjectName(QStringLiteral("acceptButton"));
    acceptButton->setEnabled(ok);
    buttonRow->addWidget(rejectButton);
    buttonRow->addWidget(counterButton);
    buttonRow->addStretch(1);
    buttonRow->addWidget(acceptButton);
    root->addLayout(buttonRow);

    connect(rejectButton, &QPushButton::clicked, &dialog, [&dialog] { dialog.done(0); });
    connect(counterButton, &QPushButton::clicked, &dialog, [&dialog] { dialog.done(2); });
    connect(acceptButton, &QPushButton::clicked, &dialog, [&dialog] { dialog.done(1); });

    const int result = dialog.exec();
    if (result == 1) {
        return TradeDecision::Accepted;
    }

    if (result == 2 && counterProposal) {
        TradeDialog counterDialog(this);
        counterDialog.setController(m_controller);
        counterDialog.setPlayers(proposal.rightPlayerId, proposal.leftPlayerId);
        counterDialog.setProposal(swappedProposal(proposal));
        counterDialog.setWindowTitle(QStringLiteral("讨价还价：重新填写交易条件"));
        if (counterDialog.exec() == QDialog::Accepted) {
            *counterProposal = counterDialog.proposal();
            return TradeDecision::CounterOffer;
        }
    }

    return TradeDecision::Rejected;
}

void MainWindow::handleIncomingTradeProposal(const QJsonObject& data, const QString& fromName)
{
    const QString proposalId = data.value(QStringLiteral("proposalId")).toString();
    const int targetPlayerId = data.value(QStringLiteral("targetPlayerId")).toInt(-1);
    const int fromPlayerId = data.value(QStringLiteral("fromPlayerId")).toInt(-1);
    const int localPlayerId = m_controller->localPlayerId();
    const bool isCounterOffer = data.value(QStringLiteral("isCounterOffer")).toBool(false);
    const TradeProposal proposal = proposalFromJson(data.value(QStringLiteral("proposal")).toObject());

    if (targetPlayerId != localPlayerId) {
        if (m_logPanel && fromPlayerId > 0) {
            const Player* from = m_controller->playerById(fromPlayerId);
            const Player* target = m_controller->playerById(targetPlayerId);
            m_logPanel->appendMessage(QStringLiteral("%1 向 %2 发起了%3，等待对方确认。")
                                      .arg(from ? from->name : (fromName.isEmpty() ? QStringLiteral("其他玩家") : fromName))
                                      .arg(target ? target->name : QStringLiteral("另一名玩家"))
                                      .arg(isCounterOffer ? QStringLiteral("一份讨价还价的新报价") : QStringLiteral("一笔交易")));
        }
        return;
    }

    QString reason;
    if (!m_controller->validateTrade(proposal, &reason)) {
        if (m_logPanel) {
            m_logPanel->appendMessage(QStringLiteral("收到交易请求，但当前无法接受：%1").arg(reason));
        }
        sendTradeRejectedToNetwork(proposalId, reason);
        return;
    }

    TradeProposal counterProposal;
    const TradeDecision decision = askTradeApproval(proposal, fromName, &counterProposal);
    if (decision == TradeDecision::Rejected) {
        const QString reasonText = QStringLiteral("对方拒绝交易");
        if (m_logPanel) {
            m_logPanel->appendMessage(QStringLiteral("已拒绝交易请求：\n%1")
                                      .arg(proposalSummary(m_controller, proposal)));
        }
        sendTradeRejectedToNetwork(proposalId, reasonText);
        ToastMessage::showMessage(this, QStringLiteral("已拒绝交易。"));
        return;
    }

    if (decision == TradeDecision::CounterOffer) {
        sendCounterOfferToNetwork(counterProposal, proposalId);
        if (m_logPanel) {
            m_logPanel->appendMessage(QStringLiteral("已发出讨价还价的新条件：\n%1")
                                      .arg(proposalSummary(m_controller, counterProposal)));
        }
        return;
    }

    QString error;
    if (!m_controller->applyTrade(proposal, &error)) {
        sendTradeRejectedToNetwork(proposalId, error);
        ToastMessage::showMessage(this, error);
        return;
    }

    QJsonObject response;
    response[QStringLiteral("proposalId")] = proposalId;
    response[QStringLiteral("proposal")] = proposalToJson(proposal);
    response[QStringLiteral("acceptedByPlayerId")] = localPlayerId;
    response[QStringLiteral("state")] = m_controller->exportStateToJson();
    m_networkBridge->sendGameAction(QStringLiteral("tradeAccepted"), response);

    if (m_logPanel) {
        m_logPanel->appendMessage(QStringLiteral("已同意交易，交易成立：\n%1")
                                  .arg(proposalSummary(m_controller, proposal)));
    }
    ToastMessage::showMessage(this, QStringLiteral("已同意交易，交易完成。"));
}

void MainWindow::handleIncomingTradeAccepted(const QJsonObject& data, const QString& fromName)
{
    const QJsonObject proposalObject = data.value(QStringLiteral("proposal")).toObject();
    const QJsonObject stateObject = data.value(QStringLiteral("state")).toObject();

    if (stateObject.isEmpty() && !proposalObject.isEmpty()) {
        
        const TradeProposal proposal = proposalFromJson(proposalObject);
        QString error;
        if (!m_controller->applyTrade(proposal, &error) && m_logPanel) {
            m_logPanel->appendMessage(QStringLiteral("交易已被同意，但本机应用失败：%1").arg(error));
        }
    }

    ToastMessage::showMessage(this, QStringLiteral("%1 已同意交易，交易成立。")
                              .arg(fromName.isEmpty() ? QStringLiteral("对方") : fromName));
    if (m_logPanel && !proposalObject.isEmpty()) {
        const TradeProposal proposal = proposalFromJson(proposalObject);
        m_logPanel->appendMessage(QStringLiteral("%1 已同意交易，交易成立：\n%2")
                                  .arg(fromName.isEmpty() ? QStringLiteral("对方") : fromName)
                                  .arg(proposalSummary(m_controller, proposal)));
    }
}

void MainWindow::handleIncomingTradeRejected(const QString&, const QString& reason, const QString& fromName)
{
    ToastMessage::showMessage(this, QStringLiteral("%1 拒绝交易：%2")
                              .arg(fromName.isEmpty() ? QStringLiteral("对方") : fromName)
                              .arg(reason));
    if (m_logPanel) {
        m_logPanel->appendMessage(QStringLiteral("%1 拒绝交易：%2")
                                  .arg(fromName.isEmpty() ? QStringLiteral("对方") : fromName)
                                  .arg(reason));
    }
}

void MainWindow::openBuildDialog()
{
    if (m_controller->phase() != GamePhase::Build) {
        ToastMessage::showMessage(this, QStringLiteral("只有建设阶段可以建设"));
        return;
    }

    const int playerId = m_activeBuildPlayerId > 0 ? m_activeBuildPlayerId : m_controller->currentPlayerId();
    if (m_networkBridge && m_networkBridge->isInRoom() && playerId != m_controller->localPlayerId()) {
        const Player* current = m_controller->playerById(playerId);
        ToastMessage::showMessage(this, QStringLiteral("当前轮到 %1 建设，请等待。")
                                  .arg(current ? current->name : QStringLiteral("其他玩家")));
        return;
    }

    handleBuildChoiceRequested(playerId);
}

void MainWindow::handleBuildChoiceRequested(int playerId)
{
    if (m_networkBridge && m_networkBridge->isInRoom() && playerId != m_controller->localPlayerId()) {
        const Player* player = m_controller->playerById(playerId);
        m_activeBuildPlayerId = -1;
        m_boardView->clearTileEffects();
        updateTradeEndUi();
        if (m_logPanel) {
            m_logPanel->appendMessage(QStringLiteral("当前轮到 %1 建设，本客户端不能代建。")
                                      .arg(player ? player->name : QStringLiteral("其他玩家")));
        }
        return;
    }

    const Player* player = m_controller->playerById(playerId);
    if (!player) {
        m_controller->finishBuildChoicesForPlayer(playerId);
        if (m_networkBridge && m_networkBridge->isInRoom()) {
            m_networkBridge->broadcastStateNow(QStringLiteral("buildFinished"));
        }
        return;
    }

    if (!m_controller->playerHasBuildOptions(playerId)) {
        m_activeBuildPlayerId = -1;
        m_boardView->clearTileEffects();
        m_controller->finishBuildChoicesForPlayer(playerId);
        if (m_networkBridge && m_networkBridge->isInRoom()) {
            m_networkBridge->broadcastStateNow(QStringLiteral("buildFinishedNoOptions"));
        }
        return;
    }

    m_activeTileChoicePlayerId = -1;
    m_activeTileChoiceChooseCount = 0;
    m_activeTileChoiceCandidates.clear();
    m_activeTileChoiceSelected.clear();
    m_activeBuildPlayerId = playerId;

    const QList<int> candidates = m_controller->buildableTilesForPlayer(playerId);
    m_boardView->clearTileEffects();
    m_boardView->showBuildableTiles(candidates);

    const QString message = QStringLiteral("轮到 %1 建设：直接点击地图上的可建设空地块，先预览商铺图片再确认建设；不建设或建完后点击右侧完成/跳过。")
                                .arg(player->name);
    ToastMessage::showMessage(this, message);
    if (m_logPanel) {
        m_logPanel->appendMessage(message);
    }
    updateTradeEndUi();
}

void MainWindow::finishBuildForLocalPlayer()
{
    if (m_controller->phase() != GamePhase::Build) {
        ToastMessage::showMessage(this, QStringLiteral("当前不是建设阶段。"));
        return;
    }

    int playerId = m_activeBuildPlayerId;
    if (playerId <= 0) {
        playerId = m_controller->currentPlayerId();
    }

    if (m_networkBridge && m_networkBridge->isInRoom() && playerId != m_controller->localPlayerId()) {
        ToastMessage::showMessage(this, QStringLiteral("当前不是本客户端玩家建设。"));
        return;
    }

    m_activeBuildPlayerId = -1;
    m_boardView->clearTileEffects();
    updateTradeEndUi();

    m_controller->finishBuildChoicesForPlayer(playerId);
    if (m_networkBridge && m_networkBridge->isInRoom()) {
        m_networkBridge->broadcastStateNow(QStringLiteral("buildFinishedByButton"));
    }
}

void MainWindow::requestEndTradeForLocalPlayer()
{
    if (m_controller->phase() != GamePhase::Trade) {
        ToastMessage::showMessage(this, QStringLiteral("只有交易阶段可以结束交易"));
        return;
    }

    const int localPlayerId = m_controller->localPlayerId();
    if (m_tradeEndVotes.contains(localPlayerId)) {
        ToastMessage::showMessage(this, QStringLiteral("你已经选择结束交易，等待其他玩家。"));
        return;
    }

    handleTradeEndVote(localPlayerId, QStringLiteral("本机玩家"));

    const bool networked = m_networkBridge && m_networkBridge->isInRoom();
    if (!networked) {
        for (const Player& player : m_controller->players()) {
            if (player.active && player.id != localPlayerId && !m_tradeEndVotes.contains(player.id)) {
                handleTradeEndVote(player.id, player.name);
            }
        }
        return;
    }

    QJsonObject data;
    data[QStringLiteral("playerId")] = localPlayerId;
    m_networkBridge->sendGameAction(QStringLiteral("tradeEndVote"), data);
}

void MainWindow::handleTradeEndVote(int playerId, const QString& playerName)
{
    if (m_controller->phase() != GamePhase::Trade || playerId <= 0) {
        return;
    }

    if (!m_tradeEndVotes.contains(playerId)) {
        m_controller->markTradeFinishedForPlayer(playerId);
        m_tradeEndVotes.insert(playerId);
        const Player* player = m_controller->playerById(playerId);
        if (m_logPanel) {
            m_logPanel->appendMessage(QStringLiteral("%1 已选择结束交易。")
                                      .arg(player ? player->name : (playerName.isEmpty() ? QStringLiteral("某玩家") : playerName)));
        }
    }

    updateTradeEndUi();

    if (allActivePlayersEndedTrade()) {
        const bool networked = m_networkBridge && m_networkBridge->isInRoom();
        const bool thisClientAdvances = !networked || m_controller->localPlayerId() == 1;
        if (m_logPanel) {
            m_logPanel->appendMessage(thisClientAdvances
                ? QStringLiteral("所有玩家都已选择结束交易，进入下一阶段。")
                : QStringLiteral("所有玩家都已选择结束交易，等待玩家 1 客户端推进阶段。"));
        }
        if (thisClientAdvances) {
            m_controller->finishTradeAndAdvanceRound();
            if (networked) {
                m_networkBridge->broadcastStateNow(QStringLiteral("allTradeEnded"));
            }
        }
    }
}

bool MainWindow::allActivePlayersEndedTrade() const
{
    const QList<Player> players = m_controller->players();
    if (players.isEmpty()) {
        return false;
    }
    for (const Player& player : players) {
        if (player.active && !m_tradeEndVotes.contains(player.id)) {
            return false;
        }
    }
    return true;
}

void MainWindow::resetTradeEndVotesIfNeeded()
{
    if (m_controller->phase() != GamePhase::Trade && !m_tradeEndVotes.isEmpty()) {
        m_tradeEndVotes.clear();
    }
}

void MainWindow::updateTradeEndUi()
{
    const GamePhase phase = m_controller->phase();
    const bool inTrade = phase == GamePhase::Trade;
    const int localPlayerId = m_controller->localPlayerId();
    const bool localEnded = m_tradeEndVotes.contains(localPlayerId);
    int activeCount = 0;
    for (const Player& player : m_controller->players()) {
        if (player.active) {
            ++activeCount;
        }
    }

    if (m_actionPanel) {
        m_actionPanel->setPhase(phase);
        m_actionPanel->setTradeClosedForLocalPlayer(localEnded);
    }

    if (!m_endTradeButton) {
        return;
    }

    if (phase == GamePhase::DrawTile) {
        const bool hasActiveChoice = m_activeTileChoicePlayerId > 0;
        m_endTradeButton->setEnabled(hasActiveChoice && m_activeTileChoiceSelected.size() == m_activeTileChoiceChooseCount);
        if (hasActiveChoice) {
            const Player* player = m_controller->playerById(m_activeTileChoicePlayerId);
            m_endTradeButton->setText(QStringLiteral("确认地块 %1/%2（%3）")
                                      .arg(m_activeTileChoiceSelected.size())
                                      .arg(m_activeTileChoiceChooseCount)
                                      .arg(player ? player->name : QStringLiteral("玩家")));
        } else {
            m_endTradeButton->setText(QStringLiteral("等待地块选择"));
        }
        return;
    }

    if (phase == GamePhase::Build) {
        const bool canFinishBuild = m_activeBuildPlayerId > 0 &&
            (!m_networkBridge || !m_networkBridge->isInRoom() || m_activeBuildPlayerId == localPlayerId);
        m_endTradeButton->setEnabled(canFinishBuild);
        if (m_activeBuildPlayerId > 0) {
            const Player* player = m_controller->playerById(m_activeBuildPlayerId);
            m_endTradeButton->setText(QStringLiteral("完成/跳过建设（%1）")
                                      .arg(player ? player->name : QStringLiteral("玩家")));
        } else {
            m_endTradeButton->setText(QStringLiteral("等待建设玩家"));
        }
        return;
    }

    if (inTrade) {
        m_endTradeButton->setEnabled(!localEnded);
        if (localEnded) {
            m_endTradeButton->setText(QStringLiteral("等待其他玩家 %1/%2").arg(m_tradeEndVotes.size()).arg(activeCount));
        } else {
            m_endTradeButton->setText(QStringLiteral("结束交易 %1/%2").arg(m_tradeEndVotes.size()).arg(activeCount));
        }
        return;
    }

    m_endTradeButton->setEnabled(false);
    m_endTradeButton->setText(QStringLiteral("等待下一阶段"));
}

void MainWindow::showPlayerInfoDialog(int playerId)
{
    const Player* player = m_controller->playerById(playerId);
    if (!player) {
        return;
    }

    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("playerAssetDialog"));
    dialog.setWindowTitle(QStringLiteral("玩家资产详情"));
    dialog.resize(620, 650);
    dialog.setStyleSheet(QStringLiteral(
        "QDialog#playerAssetDialog {"
        " background: #2a1e15;"
        " border: 4px solid #c79b5a;"
        " border-radius: 24px;"
        " }"
        "QFrame#infoFrame {"
        " background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(81,58,40,238), stop:1 rgba(49,34,24,238));"
        " border: 2px solid #e5c07d;"
        " border-radius: 18px;"
        " }"
        "QLabel { color: #fce7b2; border: none; }"
        "QLabel#dialogTitle { color: #ffe8ad; font-size: 24px; font-weight: bold; }"
        "QLabel#summaryLabel { color: #fff2d0; font-size: 16px; line-height: 140%; }"
        "QListWidget { background: rgba(255,244,220,0.95); color: #3d3329; border: 2px solid #b48a55; border-radius: 10px; padding: 6px; }"
        "QPushButton { background: #6b3f2b; color: #fff1c4; border: 1px solid #d2a869; border-radius: 8px; padding: 8px 14px; font-weight: 700; }"
        "QPushButton:hover { background: #845036; border-color: #f1d08c; }"));

    auto* root = new QVBoxLayout(&dialog);
    root->setContentsMargins(16, 16, 16, 16);

    auto* title = new QLabel(QStringLiteral("玩家资产详情"), &dialog);
    title->setObjectName(QStringLiteral("dialogTitle"));
    title->setAlignment(Qt::AlignCenter);
    root->addWidget(title);

    auto* contentFrame = new QFrame(&dialog);
    contentFrame->setObjectName(QStringLiteral("infoFrame"));
    auto* layout = new QVBoxLayout(contentFrame);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    const bool isLocal = player->id == m_controller->localPlayerId();
    auto* summary = new QLabel(
        QStringLiteral("%1%2\n资金：%3\n拥有地块：%4 个\n已建设商铺：%5 个")
            .arg(player->name)
            .arg(isLocal ? QStringLiteral("（本机）") : QString())
            .arg(isLocal ? QString::number(player->money) : QStringLiteral("隐藏"))
            .arg(player->ownedTileIds.size())
            .arg(player->shops.size()),
        contentFrame);
    summary->setObjectName(QStringLiteral("summaryLabel"));
    summary->setWordWrap(true);
    layout->addWidget(summary);

    auto addPlainItem = [](QListWidget* list, const QString& text) {
        list->addItem(text);
    };

    auto addIconItem = [](QListWidget* list, ShopType type, const QString& text) {
        auto* item = new QListWidgetItem(QIcon(AssetManager::instance().pixmap(QStringLiteral("shops/%1.png").arg(shopTypeKey(type)))), text);
        list->addItem(item);
    };

    auto* list = new QListWidget(contentFrame);

    addPlainItem(list, QStringLiteral("【拥有地块】"));
    if (player->ownedTileIds.isEmpty()) {
        addPlainItem(list, QStringLiteral("无"));
    } else {
        QStringList tileTexts;
        for (int tileId : player->ownedTileIds) {
            tileTexts << QString::number(tileId);
        }
        addPlainItem(list, tileTexts.join(QStringLiteral("、")));
    }

    addPlainItem(list, QStringLiteral(""));
    addPlainItem(list, QStringLiteral("【已建设商铺】"));
    if (player->shops.isEmpty()) {
        addPlainItem(list, QStringLiteral("无"));
    } else {
        for (const Shop& shop : player->shops) {
            QStringList tileTexts;
            for (int tileId : shop.tileIds) {
                tileTexts << QString::number(tileId);
            }
            addIconItem(list, shop.type,
                        QStringLiteral("%1 ｜ 位置：%2")
                            .arg(shopTypeName(shop.type))
                            .arg(tileTexts.join(QStringLiteral("、"))));
        }
    }

    addPlainItem(list, QStringLiteral(""));
    addPlainItem(list, QStringLiteral("【未建设商铺牌】"));
    bool hasCards = false;
    for (ShopType type : allShopTypes()) {
        const int count = player->shopCards.value(type, 0);
        if (count > 0) {
            hasCards = true;
            addIconItem(list, type,
                        QStringLiteral("%1 ×%2")
                            .arg(shopTypeName(type))
                            .arg(count));
        }
    }
    if (!hasCards) {
        addPlainItem(list, QStringLiteral("无"));
    }

    layout->addWidget(list, 1);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->addStretch(1);

    const int localPlayerId = m_controller->localPlayerId();
    const Player* localPlayer = m_controller->playerById(localPlayerId);
    const bool localEnded = m_tradeEndVotes.contains(localPlayerId)
                            || (localPlayer && localPlayer->tradeFinishedThisRound);
    const bool targetEnded = player->tradeFinishedThisRound;
    const bool canTradeFromInfo = (m_controller->phase() == GamePhase::Trade
                                   && player->id != localPlayerId
                                   && player->active
                                   && !localEnded
                                   && !targetEnded);

    QPushButton* tradeButton = nullptr;
    if (canTradeFromInfo) {
        tradeButton = new QPushButton(QStringLiteral("交易"), contentFrame);
        tradeButton->setToolTip(QStringLiteral("点击后进入与该玩家的交易界面"));
        buttonRow->addWidget(tradeButton);
    }

    auto* closeButton = new QPushButton(QStringLiteral("关闭"), contentFrame);
    buttonRow->addWidget(closeButton);
    layout->addLayout(buttonRow);

    root->addWidget(contentFrame, 1);

    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::reject);

    if (tradeButton) {
        connect(tradeButton, &QPushButton::clicked, &dialog, [&dialog] {
            dialog.done(2);
        });
    }

    const int result = dialog.exec();
    if (result == 2) {
        openTradeDialogWithPlayer(playerId);
    }
}

void MainWindow::showFinalSettlementDialog()
{
    if (m_finalSettlementShown) {
        return;
    }
    m_finalSettlementShown = true;

    struct Row {
        Player player;
        int shopCount = 0;
        int unbuiltCards = 0;
    };
    QList<Row> rows;
    for (const Player& player : m_controller->players()) {
        Row row;
        row.player = player;
        for (const Tile& tile : m_controller->tiles()) {
            if (tile.shopOwnerId == player.id && tile.shopType != ShopType::None) {
                ++row.shopCount;
            }
        }
        for (ShopType type : allShopTypes()) {
            row.unbuiltCards += player.shopCards.value(type, 0);
        }
        rows.append(row);
    }
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
        if (a.player.money != b.player.money) {
            return a.player.money > b.player.money;
        }
        return a.shopCount > b.shopCount;
    });

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("最终结算"));
    dialog.resize(720, 640);
    dialog.setStyleSheet(QStringLiteral(
        "QDialog { background: #2f281f; }"
        "QLabel { color: #fce7b2; }"
        "QScrollArea { background: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"
        "QPushButton { background: #6b3f2b; color: #fff1c4; border: 2px solid #b58a4a; border-radius: 8px; padding: 9px 16px; font-size: 15px; font-weight: 700; }"
        "QPushButton:hover { border-color: #f0cd7a; background: #7b4a31; }"));

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(22, 20, 22, 18);
    layout->setSpacing(14);

    auto* title = new QLabel(QStringLiteral("最终结算"), &dialog);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QStringLiteral("font-size: 28px; font-weight: 900; color: #ffe7a6; letter-spacing: 4px;"));
    layout->addWidget(title);

    auto* subtitle = new QLabel(QStringLiteral("胜负判定：先比较资金；资金相同则比较版图上已建设店铺数量。"), &dialog);
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setWordWrap(true);
    subtitle->setStyleSheet(QStringLiteral("font-size: 16px; color: #f8dca1; padding-bottom: 4px;"));
    layout->addWidget(subtitle);

    if (!rows.isEmpty()) {
        const Row& winner = rows.first();
        auto* winnerFrame = new QFrame(&dialog);
        winnerFrame->setStyleSheet(QStringLiteral(
            "QFrame { background: rgba(255, 224, 145, 24); border: 4px solid #f3cd74; border-radius: 18px; }"));
        auto* winnerLayout = new QVBoxLayout(winnerFrame);
        winnerLayout->setContentsMargins(18, 12, 18, 12);
        winnerLayout->setSpacing(6);

        auto* winnerLabel = new QLabel(QStringLiteral("🏆 胜利者：%1").arg(winner.player.name), winnerFrame);
        winnerLabel->setAlignment(Qt::AlignCenter);
        winnerLabel->setWordWrap(true);
        winnerLabel->setStyleSheet(QStringLiteral("font-size: 24px; font-weight: 900; color: #fff0b8; border: none; background: transparent;"));
        winnerLayout->addWidget(winnerLabel);

        auto* winnerStats = new QLabel(QStringLiteral("资金：%1    ｜    版图店铺：%2    ｜    未建店铺牌：%3")
                                       .arg(winner.player.money)
                                       .arg(winner.shopCount)
                                       .arg(winner.unbuiltCards), winnerFrame);
        winnerStats->setAlignment(Qt::AlignCenter);
        winnerStats->setWordWrap(true);
        winnerStats->setStyleSheet(QStringLiteral("font-size: 17px; font-weight: 700; color: #ffe7a6; border: none; background: transparent;"));
        winnerLayout->addWidget(winnerStats);
        layout->addWidget(winnerFrame);
    }

    auto* scroll = new QScrollArea(&dialog);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* scrollContent = new QWidget(scroll);
    auto* listLayout = new QVBoxLayout(scrollContent);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(10);

    for (int i = 0; i < rows.size(); ++i) {
        const Row& row = rows.at(i);
        const QColor color = row.player.color.isValid() ? row.player.color : QColor(201, 161, 88);
        const QString borderColor = i == 0 ? QStringLiteral("#f3cd74") : color.lighter(135).name();
        const QString rankFill = i == 0 ? QStringLiteral("#8a5b2a") : color.darker(130).name();

        auto* card = new QFrame(scrollContent);
        card->setStyleSheet(QStringLiteral(
            "QFrame { background: rgba(18, 12, 8, 34); border: 3px solid %1; border-radius: 16px; }"
            "QLabel { border: none; background: transparent; }").arg(borderColor));
        auto* cardLayout = new QHBoxLayout(card);
        cardLayout->setContentsMargins(14, 12, 14, 12);
        cardLayout->setSpacing(14);

        auto* rank = new QLabel(QStringLiteral("第\n%1\n名").arg(i + 1), card);
        rank->setAlignment(Qt::AlignCenter);
        rank->setFixedSize(68, 68);
        rank->setStyleSheet(QStringLiteral(
            "QLabel { background: %1; color: #fff0bd; border: 3px solid rgba(255, 232, 170, 170); border-radius: 34px; font-size: 17px; font-weight: 900; }").arg(rankFill));
        cardLayout->addWidget(rank);

        auto* infoLayout = new QVBoxLayout();
        infoLayout->setContentsMargins(0, 0, 0, 0);
        infoLayout->setSpacing(8);

        auto* name = new QLabel(row.player.name, card);
        name->setWordWrap(true);
        name->setStyleSheet(QStringLiteral("font-size: 21px; font-weight: 900; color: #ffedbd;"));
        infoLayout->addWidget(name);

        auto* statRow = new QHBoxLayout();
        statRow->setSpacing(8);
        const QStringList stats = {
            QStringLiteral("资金\n%1").arg(row.player.money),
            QStringLiteral("版图店铺\n%1").arg(row.shopCount),
            QStringLiteral("未建店铺牌\n%1").arg(row.unbuiltCards)
        };
        for (const QString& stat : stats) {
            auto* statBox = new QLabel(stat, card);
            statBox->setAlignment(Qt::AlignCenter);
            statBox->setWordWrap(true);
            statBox->setMinimumHeight(56);
            statBox->setStyleSheet(QStringLiteral(
                "QLabel { background: rgba(255, 236, 184, 22); color: #ffe9ae; border: 2px solid rgba(218, 174, 98, 135); border-radius: 10px; font-size: 16px; font-weight: 800; padding: 5px 8px; }"));
            statRow->addWidget(statBox, 1);
        }
        infoLayout->addLayout(statRow);
        cardLayout->addLayout(infoLayout, 1);

        listLayout->addWidget(card);
    }
    listLayout->addStretch(1);
    scroll->setWidget(scrollContent);
    layout->addWidget(scroll, 1);

    auto* buttonRow = new QHBoxLayout();
    auto* returnButton = new QPushButton(QStringLiteral("返回开始界面"), &dialog);
    auto* closeButton = new QPushButton(QStringLiteral("关闭"), &dialog);
    buttonRow->addWidget(returnButton);
    buttonRow->addStretch(1);
    buttonRow->addWidget(closeButton);
    layout->addLayout(buttonRow);

    bool restartToStartup = false;
    connect(returnButton, &QPushButton::clicked, &dialog, [&dialog, &restartToStartup] {
        restartToStartup = true;
        dialog.accept();
    });
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    dialog.exec();

    if (!restartToStartup) {
        return;
    }

    if (m_networkBridge && m_networkBridge->isInRoom()) {
        disconnectNetwork();
    }
    if (m_localServerRunning) {
        stopLocalServer();
    }

    m_tradeEndVotes.clear();
    m_startupFinished = false;
    m_finalSettlementShown = false;
    m_startupBoundPlayerId = 1;
    setWindowOpacity(0.0);
    hide();
    QTimer::singleShot(0, this, &MainWindow::showStartupDialog);
}

void MainWindow::connectNetwork()
{
    bool ok = false;

    const int controlledPlayerId = QInputDialog::getInt(this,
                                                        QStringLiteral("绑定联网玩家编号"),
                                                        QStringLiteral("请选择本客户端控制的玩家编号。联网后不要再切换主控角色："),
                                                        m_controller->localPlayerId(),
                                                        1,
                                                        4,
                                                        1,
                                                        &ok);
    if (!ok) return;

    const QString host = QInputDialog::getText(this,
                                               QStringLiteral("连接服务器"),
                                               QStringLiteral("服务器地址：\n本机测试填 127.0.0.1；局域网联机填房主电脑 IPv4 地址。"),
                                               QLineEdit::Normal,
                                               QStringLiteral("127.0.0.1"),
                                               &ok);
    if (!ok) return;

    const int port = QInputDialog::getInt(this,
                                          QStringLiteral("连接服务器"),
                                          QStringLiteral("服务器端口："),
                                          45454,
                                          1,
                                          65535,
                                          1,
                                          &ok);
    if (!ok) return;

    const QString defaultName = QStringLiteral("玩家%1").arg(controlledPlayerId);
    const QString name = QInputDialog::getText(this,
                                               QStringLiteral("登录名称"),
                                               QStringLiteral("本客户端名称："),
                                               QLineEdit::Normal,
                                               defaultName,
                                               &ok);
    if (!ok) return;

    const QString room = QInputDialog::getText(this,
                                               QStringLiteral("加入房间"),
                                               QStringLiteral("房间号：\n所有客户端必须填写同一个房间号。"),
                                               QLineEdit::Normal,
                                               QStringLiteral("default"),
                                               &ok);
    if (!ok) return;

    m_startupNetworkHost = false;
    m_startupBoundPlayerId = controlledPlayerId;
    m_startupPlayerName = name.trimmed().isEmpty() ? defaultName : name.trimmed();
    rememberPlayerDisplayName(m_startupBoundPlayerId, m_startupPlayerName);
    m_networkBridge->connectAndJoin(host, static_cast<quint16>(port), m_startupPlayerName, room, controlledPlayerId);
    statusBar()->showMessage(QStringLiteral("已绑定本客户端为玩家 %1，正在进入联网房间...").arg(controlledPlayerId), 5000);
}

void MainWindow::disconnectNetwork()
{
    m_networkBridge->disconnectFromServer();
}

void MainWindow::syncStateToRoom()
{
    m_networkBridge->broadcastStateNow(QStringLiteral("manual"));
}

void MainWindow::startLocalServer()
{
    if (m_localServerRunning) {
        statusBar()->showMessage(QStringLiteral("本机服务器已经在运行。"), 4000);
        return;
    }

    bool ok = false;
    const int port = QInputDialog::getInt(this,
                                          QStringLiteral("启动本机服务器"),
                                          QStringLiteral("监听端口："),
                                          45454,
                                          1,
                                          65535,
                                          1,
                                          &ok);
    if (!ok) return;

    if (m_localServer && m_localServer->listen(static_cast<quint16>(port))) {
        m_localServerRunning = true;
        statusBar()->showMessage(QStringLiteral("本机服务器已启动。本机连接 127.0.0.1:%1；其他电脑连接房主 IPv4 地址:%1。")
                                 .arg(port), 8000);
        if (m_logPanel) {
            m_logPanel->appendMessage(QStringLiteral("本机服务器已启动，端口 %1。其他客户端需要连接房主电脑的局域网 IPv4 地址。")
                                      .arg(port));
        }
    } else {
        ToastMessage::showMessage(this, QStringLiteral("服务器启动失败：端口可能已被占用，或系统防火墙拦截。"));
    }
}

void MainWindow::stopLocalServer()
{
    if (!m_localServerRunning || !m_localServer) {
        statusBar()->showMessage(QStringLiteral("本机服务器没有运行。"), 3000);
        return;
    }

    m_localServer->close();
    m_localServerRunning = false;
    statusBar()->showMessage(QStringLiteral("本机服务器已停止。"), 4000);
    if (m_logPanel) {
        m_logPanel->appendMessage(QStringLiteral("本机服务器已停止。"));
    }
}

void MainWindow::switchLocalPlayerForHotseat()
{
    if (m_networkBridge && m_networkBridge->isConnected()) {
        ToastMessage::showMessage(this, QStringLiteral("联网后本客户端玩家编号已绑定，不能切换主控角色。"));
        return;
    }

    bool ok = false;
    const int playerId = QInputDialog::getInt(this,
                                              QStringLiteral("本地热座调试"),
                                              QStringLiteral("这只是单机调试：一台电脑多人轮流操作，不是联网。请选择要临时控制的玩家："),
                                              m_controller->localPlayerId(),
                                              1,
                                              5,
                                              1,
                                              &ok);
    if (!ok) return;

    m_controller->setLocalPlayerId(playerId);
}
