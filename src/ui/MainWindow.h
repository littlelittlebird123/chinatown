#pragma once

#include <QLabel>
#include <QMainWindow>
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>

#include "core/GameTypes.h"

class ActionPanel;
class BoardView;
class GameController;
class GameLogPanel;
class NetworkGameBridge;
class GameServer;
class HandPanel;
class PlayerPanel;
class TileInfoPanel;
class QDialog;
class QPushButton;
class QTextEdit;
class QJsonObject;
class QMediaPlayer;
class QAudioOutput;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    enum class TradeDecision { Accepted, Rejected, CounterOffer };

private slots:
    void refreshAll();
    void openTradeDialog();
    void openTradeDialogWithPlayer(int targetPlayerId);
    void openBuildDialog();
    void handleTileChoiceRequested(int playerId, QList<int> candidateTileIds, int chooseCount);
    void handleBuildChoiceRequested(int playerId);
    void connectNetwork();
    void disconnectNetwork();
    void syncStateToRoom();
    void startLocalServer();
    void stopLocalServer();
    void switchLocalPlayerForHotseat();
    void openLogDialog();
    void showPhaseTransition(GamePhase phase);
    void handleBoardTileClicked(int tileId);
    void confirmTileChoicesFromMap();
    void finishBuildForLocalPlayer();
    void showStartupDialog();
    void handleNetworkRoomJoined(const QString& roomId);
    void handleIncomingTradeProposal(const QJsonObject& data, const QString& fromName);
    void handleIncomingTradeAccepted(const QJsonObject& data, const QString& fromName);
    void handleIncomingTradeRejected(const QString& proposalId, const QString& reason, const QString& fromName);
    void handleTradeEndVote(int playerId, const QString& playerName);
    void handleRemotePlayerDisplayName(int playerId, const QString& displayName);
    void requestEndTradeForLocalPlayer();
    void showPlayerInfoDialog(int playerId);
    void showFinalSettlementDialog();
    void openChatMessageDialog();

private:
    void setupUi();
    void setupConnections();
    void updateTradeEndUi();
    bool allActivePlayersEndedTrade() const;
    void resetTradeEndVotesIfNeeded();
    void sendTradeProposalToNetwork(const TradeProposal& proposal);
    void sendTradeRejectedToNetwork(const QString& proposalId, const QString& reason);
    TradeDecision askTradeApproval(const TradeProposal& proposal, const QString& fromName, TradeProposal* counterProposal = nullptr);
    void sendCounterOfferToNetwork(const TradeProposal& proposal, const QString& basedOnProposalId = QString());
    void resetMapInteractionState();
    void rememberPlayerDisplayName(int playerId, const QString& displayName);
    void applyKnownPlayerDisplayNames();
    void broadcastLocalPlayerDisplayName();
    QString chooseBoardTheme(const QString& title);
    void appendChatMessage(const QString& sender, const QString& target, const QString& text, bool outgoing);
    void initializeBgm();
    void updateBgmForCurrentRound();
    void setBgmVolume(qreal volume);
    void reduceBgmVolume();

    QLabel* m_topBar = nullptr;
    QLabel* m_phaseBanner = nullptr;
    QPushButton* m_logButton = nullptr;
    QPushButton* m_incomeButton = nullptr;
    QPushButton* m_endTradeButton = nullptr;
    QTextEdit* m_chatHistory = nullptr;
    QPushButton* m_chatSendButton = nullptr;
    QPushButton* m_bgmVolumeButton = nullptr;
    QMediaPlayer* m_bgmPlayer = nullptr;
    QAudioOutput* m_bgmAudioOutput = nullptr;
    int m_currentBgmIndex = -1;
    qreal m_bgmVolume = 0.45;
    GameController* m_controller = nullptr;
    BoardView* m_boardView = nullptr;
    PlayerPanel* m_playerPanel = nullptr;
    HandPanel* m_handPanel = nullptr;
    ActionPanel* m_actionPanel = nullptr;
    TileInfoPanel* m_tileInfoPanel = nullptr;
    GameLogPanel* m_logPanel = nullptr;
    QDialog* m_logDialog = nullptr;
    QDialog* m_incomeDialog = nullptr;
    NetworkGameBridge* m_networkBridge = nullptr;
    GameServer* m_localServer = nullptr;
    bool m_localServerRunning = false;
    bool m_startupNetworkHost = false;
    bool m_startupFinished = false;
    bool m_finalSettlementShown = false;
    int m_startupBoundPlayerId = 1;
    QString m_startupPlayerName;
    QMap<int, QString> m_knownPlayerDisplayNames;
    bool m_applyingKnownDisplayNames = false;
    QSet<int> m_tradeEndVotes;
    QString m_lastTopBarText;
    int m_activeTileChoicePlayerId = -1;
    int m_activeTileChoiceChooseCount = 0;
    QList<int> m_activeTileChoiceCandidates;
    QList<int> m_activeTileChoiceSelected;
    int m_activeBuildPlayerId = -1;
};
