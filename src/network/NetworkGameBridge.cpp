#include "network/NetworkGameBridge.h"

#include "core/GameController.h"
#include "network/NetworkClient.h"
#include "network/NetworkMessage.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QtGlobal>

NetworkGameBridge::NetworkGameBridge(GameController* controller, QObject* parent)
    : QObject(parent)
    , m_controller(controller)
    , m_client(new NetworkClient(this))
{
    connect(m_client, &NetworkClient::connected, this, [this] {
        emit networkMessageGenerated(QStringLiteral("已连接服务器，正在登录..."));
        m_client->login(m_pendingName.isEmpty() ? QStringLiteral("Player") : m_pendingName);
    });

    connect(m_client, &NetworkClient::disconnected, this, [this] {
        emit networkMessageGenerated(QStringLiteral("已断开服务器连接。"));
    });

    connect(m_client, &NetworkClient::connectionError, this, [this](const QString& error) {
        emit networkMessageGenerated(QStringLiteral("联网错误：%1").arg(error));
    });

    connect(m_client, &NetworkClient::loggedIn, this, [this](const QString& playerId) {
        emit networkMessageGenerated(QStringLiteral("登录成功，服务器玩家 ID：%1，正在加入房间...").arg(playerId));
        m_client->joinRoom(m_pendingRoom.isEmpty() ? QStringLiteral("default") : m_pendingRoom);
    });

    connect(m_client, &NetworkClient::roomJoined, this, [this](const QString& roomId) {
        emit networkMessageGenerated(QStringLiteral("已加入联网房间：%1。本机绑定玩家 %2。")
                                     .arg(roomId)
                                     .arg(m_boundPlayerId));
        emit roomJoined(roomId);
    });

    connect(m_client, &NetworkClient::chatReceived, this, [this](int boundPlayerId, const QString& playerName, const QString& target, const QString& text) {
        emit chatReceived(boundPlayerId, playerName, target, text);
    });

    connect(m_client, &NetworkClient::gameActionReceived,
            this, &NetworkGameBridge::handleGameAction);

    connect(m_client, &NetworkClient::messageReceived, this, [this](const NetworkMessage& message) {
        if (message.type == MessageType::PlayerJoined) {
            emit networkMessageGenerated(QStringLiteral("%1 已进入房间。").arg(message.playerName.isEmpty() ? QStringLiteral("一名玩家") : message.playerName));
        } else if (message.type == MessageType::PlayerLeft) {
            emit networkMessageGenerated(QStringLiteral("%1 已离开房间。").arg(message.playerName.isEmpty() ? QStringLiteral("一名玩家") : message.playerName));
        }
    });

    if (m_controller) {
        connect(m_controller, &GameController::stateChanged,
                this, &NetworkGameBridge::scheduleStateBroadcast);

        
        connect(m_controller, &GameController::tileChoiceRequested,
                this, [this](int playerId, QList<int> candidateTileIds, int chooseCount) {
            if (!isInRoom() || m_applyingRemoteState) {
                return;
            }
            QJsonObject data;
            data[QStringLiteral("playerId")] = playerId;
            data[QStringLiteral("chooseCount")] = chooseCount;
            QJsonArray ids;
            for (int id : candidateTileIds) {
                ids.append(id);
            }
            data[QStringLiteral("candidateTileIds")] = ids;
            data[QStringLiteral("state")] = m_controller->exportStateToJson();
            sendGameAction(QStringLiteral("tileChoicePrompt"), data);
        });

        connect(m_controller, &GameController::buildChoiceRequested,
                this, [this](int playerId) {
            if (!isInRoom() || m_applyingRemoteState) {
                return;
            }
            QJsonObject data;
            data[QStringLiteral("playerId")] = playerId;
            data[QStringLiteral("state")] = m_controller->exportStateToJson();
            sendGameAction(QStringLiteral("buildChoicePrompt"), data);
        });
    }
}

bool NetworkGameBridge::isConnected() const
{
    return m_client && m_client->isConnected();
}

bool NetworkGameBridge::isInRoom() const
{
    return isConnected() && m_client && !m_client->roomId().isEmpty();
}

QString NetworkGameBridge::roomId() const
{
    return m_client ? m_client->roomId() : QString();
}

QString NetworkGameBridge::playerName() const
{
    return m_client ? m_client->playerName() : QString();
}

QString NetworkGameBridge::playerId() const
{
    return m_client ? m_client->playerId() : QString();
}

void NetworkGameBridge::setBoundPlayerId(int playerId)
{
    m_boundPlayerId = qBound(1, playerId, 4);
    if (m_controller) {
        m_controller->setLocalPlayerId(m_boundPlayerId);
    }
}

void NetworkGameBridge::connectAndJoin(const QString& host,
                                       quint16 port,
                                       const QString& playerName,
                                       const QString& roomId,
                                       int boundPlayerId)
{
    if (!m_client) {
        return;
    }

    setBoundPlayerId(boundPlayerId);
    m_pendingName = playerName.trimmed().isEmpty() ? QStringLiteral("Player") : playerName.trimmed();
    m_pendingRoom = roomId.trimmed().isEmpty() ? QStringLiteral("default") : roomId.trimmed();

    emit networkMessageGenerated(QStringLiteral("正在连接 %1:%2 ...").arg(host).arg(port));
    m_client->connectToServer(host, port);
}

void NetworkGameBridge::disconnectFromServer()
{
    if (m_client) {
        m_client->disconnectFromServer();
    }
}

void NetworkGameBridge::sendGameAction(const QString& action, const QJsonObject& data)
{
    if (!m_client || !m_client->isConnected() || m_client->roomId().isEmpty()) {
        emit networkMessageGenerated(QStringLiteral("尚未连接并加入房间，无法发送联网操作。"));
        return;
    }

    m_client->sendGameAction(action, data);
}

void NetworkGameBridge::sendChat(const QString& text, const QString& target)
{
    if (!m_client || !m_client->isConnected() || m_client->roomId().isEmpty()) {
        emit networkMessageGenerated(QStringLiteral("尚未连接并加入房间，无法发送聊天消息。"));
        return;
    }

    m_client->sendChat(text, target, m_boundPlayerId);
}

void NetworkGameBridge::broadcastStateNow(const QString& reason)
{
    if (!m_controller || !m_client || !m_client->isConnected() || m_client->roomId().isEmpty()) {
        emit networkMessageGenerated(QStringLiteral("尚未连接并加入房间，无法同步状态。"));
        return;
    }

    QJsonObject data;
    data["reason"] = reason;
    data["state"] = m_controller->exportStateToJson();
    m_client->sendGameAction(QStringLiteral("stateSnapshot"), data);
    emit networkMessageGenerated(QStringLiteral("已向房间 %1 广播当前游戏状态。").arg(m_client->roomId()));
}

void NetworkGameBridge::scheduleStateBroadcast()
{
    if (m_applyingRemoteState || m_broadcastQueued) {
        return;
    }
    if (!m_client || !m_client->isConnected() || m_client->roomId().isEmpty()) {
        return;
    }

    m_broadcastQueued = true;
    QTimer::singleShot(250, this, [this] {
        m_broadcastQueued = false;
        if (!m_applyingRemoteState) {
            broadcastStateNow(QStringLiteral("auto"));
        }
    });
}

void NetworkGameBridge::handleGameAction(const QString& action,
                                         const QJsonObject& data,
                                         const NetworkMessage& rawMessage)
{
    if (!m_controller) {
        return;
    }

    if (m_client && !rawMessage.playerId.isEmpty() && rawMessage.playerId == m_client->playerId()) {
        return;
    }

    if (action == QStringLiteral("playerDisplayName")) {
        emit playerDisplayNameReceived(data.value(QStringLiteral("playerId")).toInt(-1),
                                       data.value(QStringLiteral("displayName")).toString());
        return;
    }

    if (action == QStringLiteral("stateSnapshot")) {
        const QJsonObject stateObject = data.value(QStringLiteral("state")).toObject();
        if (stateObject.isEmpty()) {
            return;
        }

        m_applyingRemoteState = true;
        m_controller->importStateFromJson(stateObject, true);
        m_controller->setLocalPlayerId(m_boundPlayerId);
        m_applyingRemoteState = false;

        emit networkMessageGenerated(QStringLiteral("收到 %1 的棋局同步并已应用。")
                                     .arg(rawMessage.playerName.isEmpty() ? QStringLiteral("其他玩家") : rawMessage.playerName));
        return;
    }

    if (action == QStringLiteral("tileChoicePrompt")) {
        const QJsonObject stateObject = data.value(QStringLiteral("state")).toObject();
        if (!stateObject.isEmpty()) {
            m_applyingRemoteState = true;
            m_controller->importStateFromJson(stateObject, true);
            m_controller->setLocalPlayerId(m_boundPlayerId);
            m_applyingRemoteState = false;
        }

        QList<int> candidates;
        const QJsonArray ids = data.value(QStringLiteral("candidateTileIds")).toArray();
        for (const QJsonValue& value : ids) {
            candidates.append(value.toInt());
        }
        emit remoteTileChoiceRequested(data.value(QStringLiteral("playerId")).toInt(),
                                       candidates,
                                       data.value(QStringLiteral("chooseCount")).toInt());
        return;
    }

    if (action == QStringLiteral("buildChoicePrompt")) {
        const QJsonObject stateObject = data.value(QStringLiteral("state")).toObject();
        if (!stateObject.isEmpty()) {
            m_applyingRemoteState = true;
            m_controller->importStateFromJson(stateObject, true);
            m_controller->setLocalPlayerId(m_boundPlayerId);
            m_applyingRemoteState = false;
        }

        emit remoteBuildChoiceRequested(data.value(QStringLiteral("playerId")).toInt());
        return;
    }

    if (action == QStringLiteral("tradeProposal")) {
        emit tradeProposalReceived(data, rawMessage.playerName);
        return;
    }

    if (action == QStringLiteral("tradeAccepted")) {
        const QJsonObject stateObject = data.value(QStringLiteral("state")).toObject();
        if (!stateObject.isEmpty()) {
            m_applyingRemoteState = true;
            m_controller->importStateFromJson(stateObject, true);
            m_controller->setLocalPlayerId(m_boundPlayerId);
            m_applyingRemoteState = false;
        }
        emit tradeAcceptedReceived(data, rawMessage.playerName);
        return;
    }

    if (action == QStringLiteral("tradeRejected")) {
        emit tradeRejectedReceived(data.value(QStringLiteral("proposalId")).toString(),
                                   data.value(QStringLiteral("reason")).toString(QStringLiteral("对方拒绝交易")),
                                   rawMessage.playerName);
        return;
    }

    if (action == QStringLiteral("tradeEndVote")) {
        emit tradeEndVoteReceived(data.value(QStringLiteral("playerId")).toInt(), rawMessage.playerName);
        return;
    }
}
