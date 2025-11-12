#pragma once

#include <QtWidgets/QMainWindow>

#include <optional>

#include "connection_model.hpp"
#include "listener.hpp"

class QCheckBox;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTableView;
class QLabel;

class ServerWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit ServerWindow(QWidget *parent = nullptr);

private slots:
    void handleStartStop();
    void handleConnectionUpdated(const ConnectionRow &row);
    void handleConnectionClosed(const QString &id);
    void handleFrameReceived(const QString &id, const QByteArray &payload);
    void handleInvalidPacket(const QString &id, const QString &reason);
    void handleLogMessage(const QString &text);
    void updateIntervalSettings();

private:
    void appendLog(const QString &line);
    void refreshUiState();

    Listener *listener_;
    ConnectionModel *model_;
    QTableView *connectionView_;
    QPlainTextEdit *logView_;
    QSpinBox *portSpin_;
    QPushButton *startBtn_;
    QLabel *statusLabel_;
    QLabel *statusIndicator_;  // 新增:状态指示器
    QCheckBox *intervalCheck_;
    QSpinBox *intervalSpin_;
};
