#pragma once

#include <QObject>
#include <QTcpSocket>
#include "NetworkMessage.h"

class NetworkClient : public QObject {
    Q_OBJECT

public:
    explicit NetworkClient(QObject* parent = nullptr);

    bool isConnected() const;

    QString playerId() const { return m_playerId; }
    QString playerName() const { return m_playerName; }
    QString roomId() const { return m_roomId; }

public slots:
    void connectToServer(const QString& host, quint16 port);
    void disconnectFromServer();

    void login(const QString& playerName);
    void joinRoom(const QString& roomId);
    void leaveRoom();

    void sendMessage(const NetworkMessage& message);
    void sendChat(const QString& text, const QString& target = QString(), int boundPlayerId = -1);
    void sendGameAction(const QString& action, const QJsonObject& data);

signals:
    void connected();
    void disconnected();
    void connectionError(const QString& error);

    void loggedIn(const QString& playerId);
    void roomJoined(const QString& roomId);

    void messageReceived(const NetworkMessage& message);
    void chatReceived(int boundPlayerId, const QString& playerName, const QString& target, const QString& text);
    void gameActionReceived(const QString& action,
                            const QJsonObject& data,
                            const NetworkMessage& rawMessage);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);

private:
    QTcpSocket* m_socket = nullptr;
    QByteArray m_buffer;

    QString m_playerId;
    QString m_playerName;
    QString m_roomId;

    void handleMessage(const NetworkMessage& message);
};
