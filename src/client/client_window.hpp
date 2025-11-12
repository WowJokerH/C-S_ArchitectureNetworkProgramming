#pragma once

#include <QtWidgets/QMainWindow>

#include "client_controller.hpp"

class QCheckBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;

class ClientWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit ClientWindow(QWidget *parent = nullptr);

private slots:
    void handleConnectToggle();
    void handleSendClicked();
    void handleStatusChanged(const QString &status);
    void handleLog(const QString &message);
    void handleIntervalUpdated(int value);
    void handleAutoToggled(bool checked);
    void handleStatisticsUpdated(int sent, int received);

private:
    void appendLog(const QString &line);
    QByteArray currentPayload() const;

    ClientController controller_;

    QLineEdit *hostEdit_;
    QSpinBox *portSpin_;
    QPushButton *connectBtn_;
    QLabel *statusLabel_;
    QLabel *statusIndicator_;  // 新增:状态指示器
    QPlainTextEdit *payloadEdit_;
    QPushButton *sendBtn_;
    QCheckBox *autoCheck_;
    QSpinBox *intervalSpin_;
    QLabel *serverControlled_;  // 新增:服务器控制提示
    QLabel *sentLabel_;         // 新增:发送统计
    QLabel *receivedLabel_;     // 新增:接收统计
    QPlainTextEdit *logView_;
};
