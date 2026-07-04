#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHash>
#include <QSet>
#include "network/NetworkMessage.h"

class GameServer : public QObject {
    Q_OBJECT

public:
    explicit GameServer(QObject* parent = nullptr);

    bool listen(quint16 port);
    void close();

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    struct ClientInfo {
        QTcpSocket* socket = nullptr;
        QByteArray buffer;
        QString playerId;
        QString playerName;
        QString roomId;
    };

    QTcpServer* m_server = nullptr;
    QHash<QTcpSocket*, ClientInfo*> m_clients;
    QHash<QString, QSet<QTcpSocket*>> m_rooms;

    int m_nextPlayerNumber = 1;

    void handleMessage(ClientInfo* client, const NetworkMessage& message);

    void handleLogin(ClientInfo* client, const NetworkMessage& message);
    void handleJoinRoom(ClientInfo* client, const NetworkMessage& message);
    void handleLeaveRoom(ClientInfo* client);
    void handleChat(ClientInfo* client, const NetworkMessage& message);
    void handleGameAction(ClientInfo* client, const NetworkMessage& message);

    void sendToClient(QTcpSocket* socket, const NetworkMessage& message);
    void broadcastToRoom(const QString& roomId,
                         const NetworkMessage& message,
                         bool includeSender = true,
                         QTcpSocket* sender = nullptr);

    void sendError(QTcpSocket* socket, const QString& error);
    QString makePlayerId();
};
