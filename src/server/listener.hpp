#pragma once

#include "connection_model.hpp"
#include "server_runtime.hpp"

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QTcpServer>

#include <atomic>
#include <memory>
#include <optional>
#include <unordered_map>

class SessionWorker;

class Listener : public QObject {
    Q_OBJECT

public:
    explicit Listener(QObject *parent = nullptr);
    ~Listener() override;

    bool start(quint16 port);
    void stop();
    bool isListening() const;
    quint16 port() const;

    void setForcedInterval(std::optional<int> intervalMs);
    std::optional<int> forcedInterval() const;

signals:
    void listening(quint16 port);
    void stopped();
    void connectionUpdated(const ConnectionRow &row);
    void connectionClosed(const QString &id);
    void frameReceived(const QString &id, QByteArray payload);
    void invalidPacket(const QString &id, QString reason);
    void logMessage(QString text);

private slots:
    void handleNewConnection();

private:
    void removeSession(const QString &id);

    QTcpServer *server_ = nullptr;
    std::unordered_map<QString, SessionWorker *> sessions_;
    std::unordered_map<QString, QThread *> threads_;
    std::shared_ptr<ServerRuntimeConfig> runtimeConfig_;
};
