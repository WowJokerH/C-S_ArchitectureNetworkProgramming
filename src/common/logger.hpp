#pragma once

#include <QtCore/QDateTime>
#include <QtCore/QMutex>
#include <QtCore/QObject>
#include <QtCore/QString>

namespace cs::common {

enum class LogLevel {
    Debug,
    Info,
    Warn,
    Error,
};

class Logger : public QObject {
    Q_OBJECT

public:
    static Logger &instance();

    void log(LogLevel level, const QString &category, const QString &message);

signals:
    void messageLogged(LogLevel level, QString category, QString message, QDateTime timestamp);

private:
    explicit Logger(QObject *parent = nullptr);

    QMutex mutex_;
};

}  // namespace cs::common
