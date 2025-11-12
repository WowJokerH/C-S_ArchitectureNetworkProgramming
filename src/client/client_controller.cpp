#include "client_controller.hpp"

#include <QtCore/QDateTime>

using namespace cs::protocol;

ClientController::ClientController(QObject *parent) : QObject(parent) {
    connect(&socket_, &QTcpSocket::connected, this, &ClientController::onConnected);
    connect(&socket_, &QTcpSocket::disconnected, this, &ClientController::onDisconnected);
    connect(&socket_, &QTcpSocket::readyRead, this, &ClientController::onReadyRead);
    connect(&socket_, &QTcpSocket::errorOccurred, this, &ClientController::onErrorOccurred);

    autoTimer_.setInterval(autoIntervalMs_);
    connect(&autoTimer_, &QTimer::timeout, this, &ClientController::handleAutoSend);

    reconnectTimer_.setInterval(3000);
    reconnectTimer_.setSingleShot(true);
    connect(&reconnectTimer_, &QTimer::timeout, this, &ClientController::attemptReconnect);

    ackTimer_.setInterval(ackTimeoutMs_);
    ackTimer_.setSingleShot(true);
    connect(&ackTimer_, &QTimer::timeout, this, &ClientController::handleAckTimeout);
}

void ClientController::connectToHost(const QString &host, quint16 port) {
    host_ = host;
    port_ = port;
    shouldReconnect_ = true;
    reconnectTimer_.stop();
    emit statusChanged(tr("连接中..."));
    socket_.connectToHost(host, port);
    emit logMessage(tr("[连接] 正在连接 %1:%2").arg(host).arg(port));
}

void ClientController::disconnectFromHost() {
    autoTimer_.stop();
    shouldReconnect_ = false;
    reconnectTimer_.stop();
    ackTimer_.stop();
    awaitingAck_ = false;
    socket_.disconnectFromHost();
}

void ClientController::sendPayload(const QByteArray &payload) {
    writeFrame(payload, false);
}

void ClientController::setAutoInterval(int milliseconds) {
    autoIntervalMs_ = milliseconds;
    autoTimer_.setInterval(autoIntervalMs_);
    emit logMessage(tr("[配置] 本地自动发送间隔设置为 %1 毫秒").arg(autoIntervalMs_));
}

void ClientController::setAutoPayload(const QByteArray &payload) {
    autoPayload_ = payload;
}

void ClientController::setAutoSending(bool enabled) {
    autoEnabled_ = enabled;
    if (autoEnabled_ && socket_.state() == QAbstractSocket::ConnectedState) {
        autoTimer_.start();
    } else {
        autoTimer_.stop();
    }
}

void ClientController::onConnected() {
    emit statusChanged(tr("已连接"));
    emit logMessage(tr("[连接] 成功连接到服务器"));
    emit connected();
    ackTimer_.stop();
    awaitingAck_ = false;
    sentCount_ = 0;
    receivedCount_ = 0;
    updateStatistics();
    if (autoEnabled_) {
        autoTimer_.start();
    }
}

void ClientController::onDisconnected() {
    emit statusChanged(tr("已断开"));
    emit logMessage(tr("[连接] 与服务器断开连接"));
    emit disconnected();
    autoTimer_.stop();
    ackTimer_.stop();
    awaitingAck_ = false;
    if (shouldReconnect_) {
        emit logMessage(tr("[重连] 准备在 %1 秒后重连").arg(reconnectTimer_.interval() / 1000));
        reconnectTimer_.start();
    }
}

void ClientController::onErrorOccurred(QAbstractSocket::SocketError) {
    emit statusChanged(tr("发生错误"));
    emit logMessage(tr("[错误] 网络错误: %1").arg(socket_.errorString()));
    if (shouldReconnect_ && !reconnectTimer_.isActive()) {
        emit logMessage(tr("[重连] 将在 %1 秒后尝试重连").arg(reconnectTimer_.interval() / 1000));
        reconnectTimer_.start();
    }
}

void ClientController::onReadyRead() {
    parser_.append(socket_.readAll());
    while (true) {
        FrameError error = FrameError::None;
        QString reason;
        auto frame = parser_.nextFrame(&error, &reason);
        if (!frame.has_value()) {
            if (error != FrameError::None) {
                emit logMessage(tr("[错误] 解析响应失败: %1").arg(reason));
            }
            break;
        }
        receivedCount_++;
        handleAckPayload(frame->frame.payload);
        updateStatistics();
    }
}

void ClientController::handleAutoSend() {
    if (!autoPayload_.isEmpty()) {
        if (writeFrame(autoPayload_, true)) {
            awaitingAck_ = true;
            ackTimer_.start();
        }
    }
}

void ClientController::handleAckTimeout() {
    if (!awaitingAck_) {
        return;
    }
    awaitingAck_ = false;
    emit logMessage(tr("[超时] 自动发送等待服务器响应超时,准备重连"));
    socket_.abort();
    attemptReconnect();
}

void ClientController::attemptReconnect() {
    if (!shouldReconnect_) {
        return;
    }
    if (host_.isEmpty() || port_ == 0) {
        emit logMessage(tr("[重连] 缺少重连目标配置,取消自动重连"));
        return;
    }
    ackTimer_.stop();
    awaitingAck_ = false;
    emit logMessage(tr("[重连] 正在重连 %1:%2").arg(host_).arg(port_));
    socket_.abort();
    socket_.connectToHost(host_, port_);
    emit statusChanged(tr("连接中..."));
}

QByteArray ClientController::buildRequestPayload(const QByteArray &content) {
    static quint16 msgCounter = 1;
    QByteArray payload;
    payload.append(char(0x01));  // 文本消息
    payload.append(char((msgCounter >> 8) & 0xFF));
    payload.append(char(msgCounter & 0xFF));
    payload.append(content);
    ++msgCounter;
    return payload;
}

void ClientController::handleAckPayload(const QByteArray &payload) {
    emit responseReceived(payload);
    if (payload.size() < 10) {
        emit logMessage(tr("[警告] 服务器响应长度不足"));
        return;
    }
    if (awaitingAck_) {
        awaitingAck_ = false;
        ackTimer_.stop();
    }
    const uint8_t respCode = static_cast<uint8_t>(payload.at(0));
    quint64 timestamp = 0;
    for (int i = 0; i < 8; ++i) {
        timestamp = (timestamp << 8) | static_cast<uint8_t>(payload.at(1 + i));
    }
    const uint8_t cmdId = static_cast<uint8_t>(payload.at(9));
    emit logMessage(tr("[接收] 服务器确认 | code=%1 ts=%2 cmd=%3").arg(respCode).arg(timestamp).arg(cmdId));
    if (cmdId == 0x01 && payload.size() >= 14) {
        quint32 newInterval = 0;
        for (int i = 0; i < 4; ++i) {
            newInterval = (newInterval << 8) | static_cast<uint8_t>(payload.at(10 + i));
        }
        setAutoInterval(static_cast<int>(newInterval));
        emit intervalUpdated(static_cast<int>(newInterval));
        if (autoEnabled_ && socket_.state() == QAbstractSocket::ConnectedState) {
            autoTimer_.start();
        }
    }
}

bool ClientController::writeFrame(const QByteArray &payload, bool autoMode) {
    if (socket_.state() != QAbstractSocket::ConnectedState) {
        emit logMessage(tr("[错误] 当前未连接服务器,无法发送数据"));
        if (shouldReconnect_ && !reconnectTimer_.isActive()) {
            reconnectTimer_.start();
        }
        return false;
    }
    const QByteArray frame = build_frame(kDefaultVersion, buildRequestPayload(payload));
    socket_.write(frame);
    sentCount_++;
    updateStatistics();
    
    if (autoMode) {
        emit logMessage(tr("[发送] 自动发送 %1 字节").arg(payload.size()));
    } else {
        emit logMessage(tr("[发送] 已发送 %1 字节").arg(payload.size()));
    }
    return true;
}

void ClientController::updateStatistics() {
    emit statisticsUpdated(sentCount_, receivedCount_);
}
