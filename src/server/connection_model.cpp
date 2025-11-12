#include "connection_model.hpp"

#include <QtCore/QTimeZone>

ConnectionModel::ConnectionModel(QObject *parent) : QAbstractTableModel(parent) {}

int ConnectionModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return rows_.size();
}

int ConnectionModel::columnCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return ColumnCount;
}

QVariant ConnectionModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= rows_.size()) {
        return {};
    }

    const auto &row = rows_.at(index.row());
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case Id:
                return row.id;
            case Address:
                return row.address;
            case Port:
                return row.port;
            case Status:
                return row.status;
            case LastActive:
                return row.lastActive.toString(Qt::ISODate);
            case Interval:
                return row.intervalMs;
            default:
                return {};
        }
    }
    return {};
}

QVariant ConnectionModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
            case Id:
                return QStringLiteral("连接ID");
            case Address:
                return QStringLiteral("IP地址");
            case Port:
                return QStringLiteral("端口");
            case Status:
                return QStringLiteral("状态");
            case LastActive:
                return QStringLiteral("最近活动(UTC)");
            case Interval:
                return QStringLiteral("发送间隔(ms)");
            default:
                return {};
        }
    }
    return QAbstractTableModel::headerData(section, orientation, role);
}

void ConnectionModel::upsert(const ConnectionRow &row) {
    const int idx = findRow(row.id);
    if (idx == -1) {
        beginInsertRows(QModelIndex(), rows_.size(), rows_.size());
        rows_.push_back(row);
        endInsertRows();
    } else {
        rows_[idx] = row;
        const QModelIndex top = index(idx, 0);
        const QModelIndex bottom = index(idx, ColumnCount - 1);
        emit dataChanged(top, bottom);
    }
}

void ConnectionModel::markDisconnected(const QString &id) {
    const int idx = findRow(id);
    if (idx == -1) {
        return;
    }
    rows_[idx].status = QStringLiteral("Disconnected");
    const QModelIndex top = index(idx, 0);
    const QModelIndex bottom = index(idx, ColumnCount - 1);
    emit dataChanged(top, bottom);
}

int ConnectionModel::findRow(const QString &id) const {
    for (int i = 0; i < rows_.size(); ++i) {
        if (rows_.at(i).id == id) {
            return i;
        }
    }
    return -1;
}
