#include "logger.hpp"

#include <QtCore/QMutexLocker>

namespace cs::common {

Logger::Logger(QObject *parent) : QObject(parent) {}

Logger &Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::log(LogLevel level, const QString &category, const QString &message) {
    const QDateTime timestamp = QDateTime::currentDateTimeUtc();
    QMutexLocker locker(&mutex_);
    emit messageLogged(level, category, message, timestamp);
}

}  // namespace cs::common
