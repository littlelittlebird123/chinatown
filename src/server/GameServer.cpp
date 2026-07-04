#include "GameServer.h"

#include <QDebug>
#include <QHostAddress>

GameServer::GameServer(QObject* parent)
    : QObject(parent),
      m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection,
            this, &GameServer::onNewConnection);
}

bool GameServer::listen(quint16 port)
{
    if (m_server->isListening()) {
        m_server->close();
    }

    
    
    
    bool ok = m_server->listen(QHostAddress::AnyIPv4, port);
    if (ok) {
        qInfo() << "Chinatown server listening on 0.0.0.0:" << port;
    } else {
        qWarning() << "Server listen failed:" << m_server->errorString();
    }
    return ok;
}

void GameServer::close()
{
    m_server->close();

    const auto clients = m_clients.values();
    for (ClientInfo* client : clients) {
        if (client && client->socket) {
            client->socket->disconnectFromHost();
        }
    }

    m_rooms.clear();
}

void GameServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket* socket = m_server->nextPendingConnection();

        auto* client = new ClientInfo;
        client->socket = socket;
        m_clients.insert(socket, client);

        connect(socket, &QTcpSocket::readyRead,
                this, &GameServer::onReadyRead);

        connect(socket, &QTcpSocket::disconnected,
                this, &GameServer::onDisconnected);

        NetworkMessage hello;
        hello.type = MessageType::Hello;
        hello.payload["message"] = "Welcome to Chinatown server";
        sendToClient(socket, hello);

        qInfo() << "Client connected:" << socket->peerAddress().toString()
                << socket->peerPort();
    }
}

void GameServer::onReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket || !m_clients.contains(socket)) return;

    ClientInfo* client = m_clients.value(socket);
    client->buffer.append(socket->readAll());

    NetworkMessage message;
    while (NetworkMessage::tryReadPacket(client->buffer, &message)) {
        handleMessage(client, message);
    }
}

void GameServer::onDisconnected()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket || !m_clients.contains(socket)) return;

    ClientInfo* client = m_clients.take(socket);

    QString oldRoom = client->roomId;
    QString oldPlayerId = client->playerId;
    QString oldPlayerName = client->playerName;

    handleLeaveRoom(client);

    if (!oldRoom.isEmpty()) {
        NetworkMessage left;
        left.type = MessageType::PlayerLeft;
        left.roomId = oldRoom;
        left.playerId = oldPlayerId;
        left.playerName = oldPlayerName;
        left.payload["message"] = oldPlayerName + " left room";
        broadcastToRoom(oldRoom, left, true, nullptr);
    }

    client->socket->deleteLater();
    delete client;
}

void GameServer::handleMessage(ClientInfo* client, const NetworkMessage& message)
{
    switch (message.type) {
    case MessageType::Hello:
        break;
    case MessageType::Login:
        handleLogin(client, message);
        break;
    case MessageType::JoinRoom:
        handleJoinRoom(client, message);
        break;
    case MessageType::LeaveRoom:
        handleLeaveRoom(client);
        break;
    case MessageType::Chat:
        handleChat(client, message);
        break;
    case MessageType::GameAction:
        handleGameAction(client, message);
        break;
    default:
        sendError(client->socket, "Unsupported message type");
        break;
    }
}

void GameServer::handleLogin(ClientInfo* client, const NetworkMessage& message)
{
    if (client->playerId.isEmpty()) {
        client->playerId = makePlayerId();
    }

    client->playerName = message.playerName.isEmpty()
        ? client->playerId
        : message.playerName;

    NetworkMessage reply;
    reply.type = MessageType::Login;
    reply.playerId = client->playerId;
    reply.playerName = client->playerName;
    reply.payload["ok"] = true;
    sendToClient(client->socket, reply);

    qInfo() << "Login:" << client->playerName << client->playerId;
}

void GameServer::handleJoinRoom(ClientInfo* client, const NetworkMessage& message)
{
    if (client->playerId.isEmpty()) {
        sendError(client->socket, "Please login first");
        return;
    }

    if (!client->roomId.isEmpty()) {
        handleLeaveRoom(client);
    }

    client->roomId = message.roomId.isEmpty() ? "default" : message.roomId;
    m_rooms[client->roomId].insert(client->socket);

    NetworkMessage reply;
    reply.type = MessageType::JoinRoom;
    reply.roomId = client->roomId;
    reply.playerId = client->playerId;
    reply.playerName = client->playerName;
    reply.payload["ok"] = true;
    sendToClient(client->socket, reply);

    NetworkMessage joined;
    joined.type = MessageType::PlayerJoined;
    joined.roomId = client->roomId;
    joined.playerId = client->playerId;
    joined.playerName = client->playerName;
    joined.payload["message"] = client->playerName + " joined room";
    broadcastToRoom(client->roomId, joined, true, client->socket);

    qInfo() << client->playerName << "joined room" << client->roomId;
}

void GameServer::handleLeaveRoom(ClientInfo* client)
{
    if (client->roomId.isEmpty()) return;

    QString roomId = client->roomId;

    if (m_rooms.contains(roomId)) {
        m_rooms[roomId].remove(client->socket);
        if (m_rooms[roomId].isEmpty()) {
            m_rooms.remove(roomId);
        }
    }

    client->roomId.clear();
}

void GameServer::handleChat(ClientInfo* client, const NetworkMessage& message)
{
    if (client->roomId.isEmpty()) {
        sendError(client->socket, "Join a room before chat");
        return;
    }

    NetworkMessage outgoing = message;
    outgoing.type = MessageType::Chat;
    outgoing.roomId = client->roomId;
    outgoing.playerId = client->playerId;
    outgoing.playerName = client->playerName;
    broadcastToRoom(client->roomId, outgoing, true, client->socket);
}

void GameServer::handleGameAction(ClientInfo* client, const NetworkMessage& message)
{
    if (client->roomId.isEmpty()) {
        sendError(client->socket, "Join a room before game action");
        return;
    }

    NetworkMessage outgoing = message;
    outgoing.type = MessageType::GameAction;
    outgoing.roomId = client->roomId;
    outgoing.playerId = client->playerId;
    outgoing.playerName = client->playerName;

    


    broadcastToRoom(client->roomId, outgoing, true, client->socket);
}

void GameServer::sendToClient(QTcpSocket* socket, const NetworkMessage& message)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) return;

    socket->write(message.toPacket());
    socket->flush();
}

void GameServer::broadcastToRoom(const QString& roomId,
                                 const NetworkMessage& message,
                                 bool includeSender,
                                 QTcpSocket* sender)
{
    if (!m_rooms.contains(roomId)) return;

    const QSet<QTcpSocket*> clients = m_rooms.value(roomId);
    for (QTcpSocket* socket : clients) {
        if (!includeSender && socket == sender) continue;
        sendToClient(socket, message);
    }
}

void GameServer::sendError(QTcpSocket* socket, const QString& error)
{
    NetworkMessage msg;
    msg.type = MessageType::Error;
    msg.payload["message"] = error;
    sendToClient(socket, msg);
}

QString GameServer::makePlayerId()
{
    return QString("P%1").arg(m_nextPlayerNumber++);
}
