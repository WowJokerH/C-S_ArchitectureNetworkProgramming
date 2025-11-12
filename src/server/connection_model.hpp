#pragma once

#include <QtCore/QAbstractTableModel>
#include <QtCore/QDateTime>
#include <QtCore/QVector>

struct ConnectionRow {
    QString id;
    QString address;
    quint16 port = 0;
    QString status;
    QDateTime lastActive;
    int intervalMs = 0;
};

class ConnectionModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        Id = 0,
        Address,
        Port,
        Status,
        LastActive,
        Interval,
        ColumnCount
    };

    explicit ConnectionModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void upsert(const ConnectionRow &row);
    void markDisconnected(const QString &id);

private:
    int findRow(const QString &id) const;
    QVector<ConnectionRow> rows_;
};
