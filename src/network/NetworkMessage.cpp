#include "NetworkMessage.h"

#include <QJsonDocument>
#include <QDataStream>
#include <QIODevice>

QString messageTypeToString(MessageType type)
{
    switch (type) {
    case MessageType::Hello:        return "hello";
    case MessageType::Login:        return "login";
    case MessageType::JoinRoom:     return "join_room";
    case MessageType::LeaveRoom:    return "leave_room";
    case MessageType::Chat:         return "chat";
    case MessageType::GameAction:   return "game_action";
    case MessageType::GameState:    return "game_state";
    case MessageType::PlayerJoined: return "player_joined";
    case MessageType::PlayerLeft:   return "player_left";
    case MessageType::Error:        return "error";
    case MessageType::Unknown:      return "unknown";
    }
    return "unknown";
}

MessageType messageTypeFromString(const QString& value)
{
    if (value == "hello") return MessageType::Hello;
    if (value == "login") return MessageType::Login;
    if (value == "join_room") return MessageType::JoinRoom;
    if (value == "leave_room") return MessageType::LeaveRoom;
    if (value == "chat") return MessageType::Chat;
    if (value == "game_action") return MessageType::GameAction;
    if (value == "game_state") return MessageType::GameState;
    if (value == "player_joined") return MessageType::PlayerJoined;
    if (value == "player_left") return MessageType::PlayerLeft;
    if (value == "error") return MessageType::Error;
    return MessageType::Unknown;
}

QJsonObject NetworkMessage::toJson() const
{
    QJsonObject obj;
    obj["type"] = messageTypeToString(type);
    obj["roomId"] = roomId;
    obj["playerId"] = playerId;
    obj["playerName"] = playerName;
    obj["payload"] = payload;
    return obj;
}

NetworkMessage NetworkMessage::fromJson(const QJsonObject& object)
{
    NetworkMessage msg;
    msg.type = messageTypeFromString(object.value("type").toString());
    msg.roomId = object.value("roomId").toString();
    msg.playerId = object.value("playerId").toString();
    msg.playerName = object.value("playerName").toString();
    msg.payload = object.value("payload").toObject();
    return msg;
}

QByteArray NetworkMessage::toPacket() const
{
    QByteArray json = QJsonDocument(toJson()).toJson(QJsonDocument::Compact);

    QByteArray packet;
    QDataStream stream(&packet, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << static_cast<quint32>(json.size());
    packet.append(json);

    return packet;
}

bool NetworkMessage::tryReadPacket(QByteArray& buffer, NetworkMessage* outMessage)
{
    constexpr int headerSize = sizeof(quint32);

    if (buffer.size() < headerSize) {
        return false;
    }

    QDataStream stream(buffer.left(headerSize));
    stream.setByteOrder(QDataStream::BigEndian);

    quint32 length = 0;
    stream >> length;

    if (buffer.size() < headerSize + static_cast<int>(length)) {
        return false;
    }

    QByteArray json = buffer.mid(headerSize, length);
    buffer.remove(0, headerSize + static_cast<int>(length));

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(json, &error);

    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        if (outMessage) {
            outMessage->type = MessageType::Error;
            outMessage->payload["message"] = "Invalid JSON packet";
        }
        return true;
    }

    if (outMessage) {
        *outMessage = NetworkMessage::fromJson(doc.object());
    }

    return true;
}
