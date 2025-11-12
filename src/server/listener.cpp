#include "listener.hpp"

#include <QtCore/QDateTime>
#include <QtCore/QStringList>
#include <QtCore/QUuid>
#include <QtCore/QVariant>
#include <QtCore/QThread>
#include <QtNetwork/QHostAddress>

#include "session_worker.hpp"

Listener::Listener(QObject *parent)
    : QObject(parent),
      server_(new QTcpServer(this)),
      runtimeConfig_(std::make_shared<ServerRuntimeConfig>()) {
    connect(server_, &QTcpServer::newConnection, this, &Listener::handleNewConnection);
}

Listener::~Listener() {
    stop();
}

bool Listener::start(quint16 port) {
    if (server_->isListening()) {
        server_->close();
    }
    if (!server_->listen(QHostAddress::Any, port)) {
        emit logMessage(QStringLiteral("监听失败：%1").arg(server_->errorString()));
        return false;
    }
    emit listening(server_->serverPort());
    return true;
}

void Listener::stop() {
    // 先记录所有会话ID
    QStringList sessionIds;
    for (auto &[id, worker] : sessions_) {
        sessionIds.append(id);
        if (worker) {
            QMetaObject::invokeMethod(worker, "stop", Qt::QueuedConnection);
        }
    }
    
    // 清理会话和线程
    sessions_.clear();
    threads_.clear();
    
    // 关闭服务器
    if (server_->isListening()) {
        server_->close();
    }
    
    // 通知所有连接已关闭（关键！）
    for (const QString &id : sessionIds) {
        emit connectionClosed(id);
    }
    
    emit stopped();
}

bool Listener::isListening() const {
    return server_ && server_->isListening();
}

quint16 Listener::port() const {
    return server_ ? server_->serverPort() : 0;
}

void Listener::setForcedInterval(std::optional<int> intervalMs) {
    runtimeConfig_->intervalControl = intervalMs.has_value();
    if (intervalMs) {
        runtimeConfig_->forcedIntervalMs = *intervalMs;
    }
}

std::optional<int> Listener::forcedInterval() const {
    if (!runtimeConfig_->intervalControl.load()) {
        return std::nullopt;
    }
    return runtimeConfig_->forcedIntervalMs.load();
}

void Listener::handleNewConnection() {
    while (server_->hasPendingConnections()) {
        auto socket = server_->nextPendingConnection();
        if (!socket) {
            continue;
        }
        const QString address = socket->peerAddress().toString();
        const quint16 peerPort = socket->peerPort();
        socket->setParent(nullptr);
        const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        auto *thread = new QThread(this);
        auto *worker = new SessionWorker(socket, id, runtimeConfig_);
        worker->moveToThread(thread);
        socket->moveToThread(thread);

        connect(thread, &QThread::started, worker, &SessionWorker::start);
        connect(worker, &SessionWorker::finished, this, [this](const QString &connectionId) {
            removeSession(connectionId);
        });
        connect(worker, &SessionWorker::finished, worker, &QObject::deleteLater);
        connect(worker, &SessionWorker::connectionUpdated, this, &Listener::connectionUpdated);
        connect(worker, &SessionWorker::frameReceived, this, &Listener::frameReceived);
        connect(worker, &SessionWorker::invalidPacket, this, &Listener::invalidPacket);
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);

        sessions_.emplace(id, worker);
        threads_.emplace(id, thread);
        thread->start();

        ConnectionRow row;
        row.id = id;
        row.address = address;
        row.port = peerPort;
        row.status = QStringLiteral("已连接");
        row.lastActive = QDateTime::currentDateTimeUtc();
        row.intervalMs = runtimeConfig_->forcedIntervalMs.load();
        emit connectionUpdated(row);
        emit logMessage(QStringLiteral("新的客户端接入 %1:%2").arg(row.address).arg(row.port));
    }
}

void Listener::removeSession(const QString &id) {
    auto itThread = threads_.find(id);
    if (itThread != threads_.end()) {
        if (itThread->second) {
            itThread->second->quit();
            itThread->second->wait(1000);
        }
        threads_.erase(itThread);
    }
    sessions_.erase(id);
    emit connectionClosed(id);
}
