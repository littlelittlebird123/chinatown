#pragma once

#include <QString>
#include <QJsonObject>
#include <QByteArray>

enum class MessageType {
    Hello,
    Login,
    JoinRoom,
    LeaveRoom,
    Chat,
    GameAction,
    GameState,
    PlayerJoined,
    PlayerLeft,
    Error,
    Unknown
};

QString messageTypeToString(MessageType type);
MessageType messageTypeFromString(const QString& value);

struct NetworkMessage {
    MessageType type = MessageType::Unknown;
    QString roomId;
    QString playerId;
    QString playerName;
    QJsonObject payload;

    QJsonObject toJson() const;
    static NetworkMessage fromJson(const QJsonObject& object);

    QByteArray toPacket() const;
    static bool tryReadPacket(QByteArray& buffer, NetworkMessage* outMessage);
};
