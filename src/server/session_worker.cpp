#include "session_worker.hpp"

#include <QtCore/QDateTime>
#include <QtCore/QThread>

#include "common/protocol.hpp"

using namespace cs::protocol;

SessionWorker::SessionWorker(QTcpSocket *socket, QString connectionId,
                             std::shared_ptr<ServerRuntimeConfig> runtime, QObject *parent)
    : QObject(parent),
      socket_(socket),
      connectionId_(std::move(connectionId)),
      runtimeConfig_(std::move(runtime)),
      parser_(std::make_unique<ProtocolParser>()) {
    currentRow_.id = connectionId_;
    if (socket_) {
        currentRow_.address = socket_->peerAddress().toString();
        currentRow_.port = socket_->peerPort();
        currentRow_.status = QStringLiteral("已连接");
        currentRow_.lastActive = QDateTime::currentDateTimeUtc();
        currentRow_.intervalMs = runtimeConfig_->forcedIntervalMs.load();
    }
}

SessionWorker::~SessionWorker() = default;

void SessionWorker::start() {
    if (!socket_) {
        emit finished(connectionId_);
        return;
    }
    connect(socket_.data(), &QTcpSocket::readyRead, this, &SessionWorker::onReadyRead);
    connect(socket_.data(), &QTcpSocket::disconnected, this, &SessionWorker::onDisconnected);
    emit connectionUpdated(currentRow_);
}

void SessionWorker::stop() {
    if (finished_) {
        return;  // 已经处理过了
    }
    
    if (socket_) {
        // 先断开readyRead信号，避免在关闭过程中继续处理数据
        disconnect(socket_.data(), &QTcpSocket::readyRead, this, &SessionWorker::onReadyRead);
        
        // 优雅地关闭连接，让客户端能检测到断开
        if (socket_->state() == QAbstractSocket::ConnectedState) {
            socket_->disconnectFromHost();
            // 等待disconnected信号触发或超时
            if (socket_->state() != QAbstractSocket::UnconnectedState) {
                socket_->waitForDisconnected(1000);
            }
        }
        
        // 如果还未断开，强制关闭
        if (socket_->state() != QAbstractSocket::UnconnectedState) {
            socket_->abort();
        }
    }
    
    // 如果onDisconnected还没触发finished，这里触发
    if (!finished_) {
        finished_ = true;
        currentRow_.status = QStringLiteral("已断开");
        currentRow_.lastActive = QDateTime::currentDateTimeUtc();
        emit connectionUpdated(currentRow_);
        emit finished(connectionId_);
    }
}

void SessionWorker::onReadyRead() {
    if (!socket_) {
        return;
    }
    parser_->append(socket_->readAll());
    while (true) {
        FrameError error = FrameError::None;
        QString reason;
        const auto frame = parser_->nextFrame(&error, &reason);
        if (!frame.has_value()) {
            if (error != FrameError::None) {
                emit invalidPacket(connectionId_, reason);
            }
            break;
        }
        currentRow_.lastActive = QDateTime::currentDateTimeUtc();
        currentRow_.status = QStringLiteral("活跃");
        emit connectionUpdated(currentRow_);
        emit frameReceived(connectionId_, frame->frame.payload);
        sendAck(true);
    }
}

void SessionWorker::onDisconnected() {
    if (finished_) {
        return;  // 已经处理过了
    }
    finished_ = true;
    currentRow_.status = QStringLiteral("已断开");
    currentRow_.lastActive = QDateTime::currentDateTimeUtc();
    emit connectionUpdated(currentRow_);
    emit finished(connectionId_);
}

void SessionWorker::sendAck(bool success) {
    if (!socket_) {
        return;
    }
    const QByteArray payload = buildAckPayload(success);
    const QByteArray frame = build_frame(kDefaultVersion, payload);
    socket_->write(frame);
}

QByteArray SessionWorker::buildAckPayload(bool success) {
    QByteArray payload;
    payload.append(char(success ? 0x00 : 0x01));
    const quint64 timestamp = static_cast<quint64>(QDateTime::currentMSecsSinceEpoch());
    for (int i = 7; i >= 0; --i) {
        payload.append(char((timestamp >> (i * 8)) & 0xFF));
    }
    bool includeInterval = runtimeConfig_->intervalControl.load();
    payload.append(char(includeInterval ? 0x01 : 0x00));
    if (includeInterval) {
        const quint32 interval = static_cast<quint32>(runtimeConfig_->forcedIntervalMs.load());
        currentRow_.intervalMs = interval;
        for (int i = 3; i >= 0; --i) {
            payload.append(char((interval >> (i * 8)) & 0xFF));
        }
    }
    return payload;
}
