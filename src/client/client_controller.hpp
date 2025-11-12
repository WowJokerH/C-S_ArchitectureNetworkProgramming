#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtNetwork/QAbstractSocket>
#include <QtNetwork/QTcpSocket>

#include <optional>

#include "common/protocol.hpp"

class ClientController : public QObject {
    Q_OBJECT

public:
    explicit ClientController(QObject *parent = nullptr);

    void connectToHost(const QString &host, quint16 port);
    void disconnectFromHost();
    void sendPayload(const QByteArray &payload);
    void setAutoInterval(int milliseconds);
    void setAutoPayload(const QByteArray &payload);
    void setAutoSending(bool enabled);

signals:
    void statusChanged(QString status);
    void logMessage(QString message);
    void responseReceived(QByteArray payload);
    void intervalUpdated(int milliseconds);
    void statisticsUpdated(int sent, int received);  // 新增:统计信号
    void connected();
    void disconnected();

private slots:
    void onConnected();
    void onDisconnected();
    void onErrorOccurred(QAbstractSocket::SocketError error);
    void onReadyRead();
    void handleAutoSend();
    void handleAckTimeout();
    void attemptReconnect();

private:
    bool writeFrame(const QByteArray &payload, bool autoMode);
    QByteArray buildRequestPayload(const QByteArray &content);
    void handleAckPayload(const QByteArray &payload);

    QTcpSocket socket_;
    QTimer autoTimer_;
    QTimer reconnectTimer_;
    QTimer ackTimer_;
    QByteArray autoPayload_;
    int autoIntervalMs_ = 3000;
    int ackTimeoutMs_ = 5000;
    bool autoEnabled_ = false;
    bool shouldReconnect_ = false;
    bool awaitingAck_ = false;
    QString host_;
    quint16 port_ = 0;
    cs::protocol::ProtocolParser parser_;
    int sentCount_ = 0;      // 新增:发送计数
    int receivedCount_ = 0;  // 新增:接收计数
    
    void updateStatistics();  // 新增:更新统计
};
