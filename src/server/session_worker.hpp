#pragma once

#include "connection_model.hpp"
#include "server_runtime.hpp"

#include <QtCore/QByteArray>
#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtCore/QUuid>
#include <QtNetwork/QTcpSocket>

#include <memory>

namespace cs::protocol {
class ProtocolParser;
struct ParsedFrame;
}  // namespace cs::protocol

class SessionWorker : public QObject {
    Q_OBJECT

public:
    SessionWorker(QTcpSocket *socket, QString connectionId, std::shared_ptr<ServerRuntimeConfig> runtime, QObject *parent = nullptr);
    ~SessionWorker() override;

public slots:
    void start();
    void stop();

signals:
    void connectionUpdated(ConnectionRow row);
    void frameReceived(QString connectionId, QByteArray payload);
    void invalidPacket(QString connectionId, QString reason);
    void finished(QString connectionId);

private slots:
    void onReadyRead();
    void onDisconnected();

private:
    void sendAck(bool success);
    QByteArray buildAckPayload(bool success);

    QScopedPointer<QTcpSocket> socket_;
    QString connectionId_;
    std::shared_ptr<ServerRuntimeConfig> runtimeConfig_;
    std::unique_ptr<cs::protocol::ProtocolParser> parser_;
    ConnectionRow currentRow_;
    bool finished_ = false;  // 防止重复触发finished信号
};
