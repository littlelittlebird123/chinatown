#include "NetworkClient.h"

#include <QNetworkProxy>

NetworkClient::NetworkClient(QObject* parent)
    : QObject(parent),
      m_socket(new QTcpSocket(this))
{
    
    
    m_socket->setProxy(QNetworkProxy::NoProxy);
    connect(m_socket, &QTcpSocket::connected,
            this, &NetworkClient::onConnected);

    connect(m_socket, &QTcpSocket::disconnected,
            this, &NetworkClient::onDisconnected);

    connect(m_socket, &QTcpSocket::readyRead,
            this, &NetworkClient::onReadyRead);

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(m_socket, &QTcpSocket::errorOccurred,
            this, &NetworkClient::onSocketError);
#else
    connect(m_socket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(onSocketError(QAbstractSocket::SocketError)));
#endif
}

bool NetworkClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void NetworkClient::connectToServer(const QString& host, quint16 port)
{
    
    
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }

    m_buffer.clear();
    m_socket->setProxy(QNetworkProxy::NoProxy);
    m_socket->connectToHost(host.trimmed(), port);
}

void NetworkClient::disconnectFromServer()
{
    m_socket->disconnectFromHost();
}

void NetworkClient::login(const QString& playerName)
{
    m_playerName = playerName;

    NetworkMessage msg;
    msg.type = MessageType::Login;
    msg.playerName = playerName;
    msg.payload["clientVersion"] = "chinatown-ui-demo-network";
    sendMessage(msg);
}

void NetworkClient::joinRoom(const QString& roomId)
{
    m_roomId = roomId;

    NetworkMessage msg;
    msg.type = MessageType::JoinRoom;
    msg.playerId = m_playerId;
    msg.playerName = m_playerName;
    msg.roomId = roomId;
    sendMessage(msg);
}

void NetworkClient::leaveRoom()
{
    NetworkMessage msg;
    msg.type = MessageType::LeaveRoom;
    msg.playerId = m_playerId;
    msg.playerName = m_playerName;
    msg.roomId = m_roomId;
    sendMessage(msg);

    m_roomId.clear();
}

void NetworkClient::sendMessage(const NetworkMessage& message)
{
    if (!isConnected()) {
        emit connectionError("未连接服务器");
        return;
    }

    m_socket->write(message.toPacket());
    m_socket->flush();
}

void NetworkClient::sendChat(const QString& text, const QString& target, int boundPlayerId)
{
    NetworkMessage msg;
    msg.type = MessageType::Chat;
    msg.playerId = m_playerId;
    msg.playerName = m_playerName;
    msg.roomId = m_roomId;
    msg.payload["text"] = text;
    if (!target.trimmed().isEmpty()) {
        msg.payload["target"] = target.trimmed();
    }
    if (boundPlayerId > 0) {
        msg.payload["boundPlayerId"] = boundPlayerId;
    }
    sendMessage(msg);
}

void NetworkClient::sendGameAction(const QString& action, const QJsonObject& data)
{
    NetworkMessage msg;
    msg.type = MessageType::GameAction;
    msg.playerId = m_playerId;
    msg.playerName = m_playerName;
    msg.roomId = m_roomId;
    msg.payload["action"] = action;
    msg.payload["data"] = data;
    sendMessage(msg);
}

void NetworkClient::onConnected()
{
    NetworkMessage hello;
    hello.type = MessageType::Hello;
    hello.payload["client"] = "ChinatownUiDemo";
    sendMessage(hello);

    emit connected();
}

void NetworkClient::onDisconnected()
{
    emit disconnected();
}

void NetworkClient::onReadyRead()
{
    m_buffer.append(m_socket->readAll());

    NetworkMessage msg;
    while (NetworkMessage::tryReadPacket(m_buffer, &msg)) {
        handleMessage(msg);
    }
}

void NetworkClient::onSocketError(QAbstractSocket::SocketError)
{
    emit connectionError(m_socket->errorString());
}

void NetworkClient::handleMessage(const NetworkMessage& message)
{
    emit messageReceived(message);

    switch (message.type) {
    case MessageType::Login:
        m_playerId = message.playerId;
        emit loggedIn(m_playerId);
        break;

    case MessageType::JoinRoom:
        m_roomId = message.roomId;
        emit roomJoined(m_roomId);
        break;

    case MessageType::Chat:
        emit chatReceived(message.payload.value("boundPlayerId").toInt(-1),
                          message.playerName,
                          message.payload.value("target").toString(),
                          message.payload.value("text").toString());
        break;

    case MessageType::GameAction:
        emit gameActionReceived(message.payload.value("action").toString(),
                                message.payload.value("data").toObject(),
                                message);
        break;

    case MessageType::Error:
        emit connectionError(message.payload.value("message").toString());
        break;

    default:
        break;
    }
}
