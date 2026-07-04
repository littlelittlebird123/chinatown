#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QJsonObject>

class GameController;
class NetworkClient;
class NetworkMessage;

class NetworkGameBridge : public QObject {
    Q_OBJECT

public:
    explicit NetworkGameBridge(GameController* controller, QObject* parent = nullptr);

    bool isConnected() const;
    bool isInRoom() const;
    QString roomId() const;
    QString playerName() const;
    QString playerId() const;
    int boundPlayerId() const { return m_boundPlayerId; }

public slots:
    void connectAndJoin(const QString& host,
                        quint16 port,
                        const QString& playerName,
                        const QString& roomId,
                        int boundPlayerId = 1);
    void disconnectFromServer();
    void broadcastStateNow(const QString& reason = QStringLiteral("manual"));
    void sendGameAction(const QString& action, const QJsonObject& data);
    void sendChat(const QString& text, const QString& target = QString());
    void setBoundPlayerId(int playerId);

signals:
    void networkMessageGenerated(const QString& message);
    void chatReceived(int boundPlayerId, const QString& playerName, const QString& target, const QString& text);
    void roomJoined(const QString& roomId);
    void remoteTileChoiceRequested(int playerId, QList<int> candidateTileIds, int chooseCount);
    void remoteBuildChoiceRequested(int playerId);
    void tradeProposalReceived(const QJsonObject& data, const QString& fromName);
    void tradeAcceptedReceived(const QJsonObject& data, const QString& fromName);
    void tradeRejectedReceived(const QString& proposalId, const QString& reason, const QString& fromName);
    void tradeEndVoteReceived(int playerId, const QString& playerName);
    void playerDisplayNameReceived(int playerId, const QString& displayName);

private slots:
    void scheduleStateBroadcast();
    void handleGameAction(const QString& action,
                          const QJsonObject& data,
                          const NetworkMessage& rawMessage);

private:
    GameController* m_controller = nullptr;
    NetworkClient* m_client = nullptr;
    QString m_pendingName;
    QString m_pendingRoom;
    int m_boundPlayerId = 1;
    bool m_applyingRemoteState = false;
    bool m_broadcastQueued = false;
};
