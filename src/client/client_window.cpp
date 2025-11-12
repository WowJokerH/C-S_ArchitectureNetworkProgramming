#include "client_window.hpp"

#include <QtCore/QDateTime>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>

ClientWindow::ClientWindow(QWidget *parent) : QMainWindow(parent) {
    auto *central = new QWidget(this);
    setCentralWidget(central);

    // 连接配置
    hostEdit_ = new QLineEdit(QStringLiteral("127.0.0.1"), central);
    hostEdit_->setMinimumWidth(150);
    
    portSpin_ = new QSpinBox(central);
    portSpin_->setRange(1024, 65535);
    portSpin_->setValue(8080);
    portSpin_->setMinimumWidth(100);

    connectBtn_ = new QPushButton(tr("连接"), central);
    connectBtn_->setMinimumWidth(100);
    connectBtn_->setStyleSheet("QPushButton { font-weight: bold; padding: 8px; }");
    
    statusLabel_ = new QLabel(tr("未连接"), central);
    statusIndicator_ = new QLabel(tr("●"), central);
    statusIndicator_->setStyleSheet("QLabel { color: red; font-size: 16pt; font-weight: bold; }");

    // 数据发送
    payloadEdit_ = new QPlainTextEdit(central);
    payloadEdit_->setPlainText(QStringLiteral("你好，服务器！"));
    payloadEdit_->setMaximumHeight(80);
    payloadEdit_->setPlaceholderText(tr("输入要发送的数据内容..."));

    sendBtn_ = new QPushButton(tr("立即发送"), central);
    sendBtn_->setMinimumWidth(120);
    sendBtn_->setEnabled(false);

    // 自动发送控制
    autoCheck_ = new QCheckBox(tr("启用自动发送"), central);
    autoCheck_->setEnabled(false);
    
    intervalSpin_ = new QSpinBox(central);
    intervalSpin_->setRange(500, 60000);
    intervalSpin_->setSingleStep(500);
    intervalSpin_->setSuffix(tr(" 毫秒"));
    intervalSpin_->setValue(3000);
    intervalSpin_->setEnabled(false);
    intervalSpin_->setMinimumWidth(120);
    
    serverControlled_ = new QLabel(tr("当前间隔由客户端控制"), central);
    serverControlled_->setStyleSheet("QLabel { color: blue; font-style: italic; }");
    
    // 统计标签
    sentLabel_ = new QLabel(tr("已发送: 0"), central);
    receivedLabel_ = new QLabel(tr("已接收: 0"), central);
    sentLabel_->setStyleSheet("QLabel { font-weight: bold; }");
    receivedLabel_->setStyleSheet("QLabel { font-weight: bold; color: green; }");
    
    // 日志视图
    logView_ = new QPlainTextEdit(central);
    logView_->setReadOnly(true);
    logView_->setMaximumBlockCount(1000);
    logView_->setStyleSheet(
        "QPlainTextEdit { "
        "   background-color: #f5f5f5; "
        "   font-family: 'Consolas', 'Courier New', monospace; "
        "   font-size: 9pt; "
        "}"
    );

    // 连接控制组
    auto *connGroup = new QGroupBox(tr("服务器连接"), central);
    auto *connLayout = new QGridLayout(connGroup);
    
    auto *hostLabel = new QLabel(tr("服务器地址:"), connGroup);
    auto *portLabel = new QLabel(tr("端口:"), connGroup);
    auto *statusLabelText = new QLabel(tr("连接状态:"), connGroup);
    
    connLayout->addWidget(hostLabel, 0, 0);
    connLayout->addWidget(hostEdit_, 0, 1);
    connLayout->addWidget(portLabel, 0, 2);
    connLayout->addWidget(portSpin_, 0, 3);
    connLayout->addWidget(connectBtn_, 0, 4);
    connLayout->addWidget(statusLabelText, 1, 0);
    connLayout->addWidget(statusIndicator_, 1, 1);
    connLayout->addWidget(statusLabel_, 1, 2, 1, 3);

    // 数据发送组
    auto *payloadGroup = new QGroupBox(tr("数据发送"), central);
    auto *payloadLayout = new QVBoxLayout(payloadGroup);
    payloadLayout->addWidget(new QLabel(tr("发送内容:"), payloadGroup));
    payloadLayout->addWidget(payloadEdit_);
    
    auto *btnLayout = new QHBoxLayout;
    btnLayout->addWidget(sendBtn_);
    btnLayout->addStretch();
    payloadLayout->addLayout(btnLayout);

    // 自动发送组
    auto *autoGroup = new QGroupBox(tr("自动发送设置"), central);
    auto *autoLayout = new QGridLayout(autoGroup);
    autoLayout->addWidget(autoCheck_, 0, 0, 1, 2);
    autoLayout->addWidget(new QLabel(tr("发送间隔:"), autoGroup), 1, 0);
    autoLayout->addWidget(intervalSpin_, 1, 1);
    autoLayout->addWidget(serverControlled_, 2, 0, 1, 2);

    // 统计信息组
    auto *statsGroup = new QGroupBox(tr("通信统计"), central);
    auto *statsLayout = new QHBoxLayout(statsGroup);
    statsLayout->addWidget(sentLabel_);
    statsLayout->addWidget(receivedLabel_);
    statsLayout->addStretch();

    // 日志组
    auto *logGroup = new QGroupBox(tr("通信日志"), central);
    auto *logLayout = new QVBoxLayout(logGroup);
    logLayout->addWidget(logView_);

    // 主布局
    auto *layout = new QVBoxLayout;
    layout->addWidget(connGroup);
    layout->addWidget(payloadGroup);
    layout->addWidget(autoGroup);
    layout->addWidget(statsGroup);
    layout->addWidget(logGroup, 1);
    layout->setSpacing(10);
    layout->setContentsMargins(10, 10, 10, 10);
    central->setLayout(layout);

    setWindowTitle(tr("C/S 客户端"));
    resize(800, 650);

    // 信号连接
    connect(connectBtn_, &QPushButton::clicked, this, &ClientWindow::handleConnectToggle);
    connect(sendBtn_, &QPushButton::clicked, this, &ClientWindow::handleSendClicked);
    connect(autoCheck_, &QCheckBox::toggled, this, &ClientWindow::handleAutoToggled);
    connect(intervalSpin_, qOverload<int>(&QSpinBox::valueChanged), &controller_, &ClientController::setAutoInterval);
    connect(payloadEdit_, &QPlainTextEdit::textChanged, this, [this]() {
        controller_.setAutoPayload(currentPayload());
    });

    connect(&controller_, &ClientController::statusChanged, this, &ClientWindow::handleStatusChanged);
    connect(&controller_, &ClientController::logMessage, this, &ClientWindow::handleLog);
    connect(&controller_, &ClientController::intervalUpdated, this, &ClientWindow::handleIntervalUpdated);
    connect(&controller_, &ClientController::statisticsUpdated, this, &ClientWindow::handleStatisticsUpdated);
    
    appendLog(tr("[系统] 客户端已就绪"));
}

void ClientWindow::handleConnectToggle() {
    const bool connected = (statusLabel_->text() == tr("已连接"));
    if (connected) {
        controller_.disconnectFromHost();
    } else {
        controller_.connectToHost(hostEdit_->text(), static_cast<quint16>(portSpin_->value()));
    }
}

void ClientWindow::handleSendClicked() {
    controller_.sendPayload(currentPayload());
}

void ClientWindow::handleStatusChanged(const QString &status) {
    statusLabel_->setText(status);
    connectBtn_->setText(status == tr("已连接") ? tr("断开") : tr("连接"));
    const bool connected = (status == tr("已连接"));
    const bool connecting = (status == tr("连接中..."));
    
    // 更新状态指示器
    if (connected) {
        statusIndicator_->setStyleSheet("QLabel { color: green; font-size: 16pt; font-weight: bold; }");
    } else if (connecting) {
        statusIndicator_->setStyleSheet("QLabel { color: orange; font-size: 16pt; font-weight: bold; }");
    } else {
        statusIndicator_->setStyleSheet("QLabel { color: red; font-size: 16pt; font-weight: bold; }");
    }
    
    portSpin_->setEnabled(!connected && !connecting);
    hostEdit_->setEnabled(!connected && !connecting);
    connectBtn_->setEnabled(!connecting || connected);
    sendBtn_->setEnabled(connected);
    autoCheck_->setEnabled(connected);
    
    if (!connected) {
        autoCheck_->setChecked(false);
        intervalSpin_->setEnabled(false);
    }
}

void ClientWindow::handleLog(const QString &message) {
    appendLog(message);
}

void ClientWindow::handleIntervalUpdated(int value) {
    intervalSpin_->blockSignals(true);
    intervalSpin_->setValue(value);
    intervalSpin_->blockSignals(false);
    serverControlled_->setText(tr("当前间隔由服务器控制: %1 毫秒").arg(value));
    serverControlled_->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    intervalSpin_->setEnabled(false);
    appendLog(tr("[配置] 服务器更新发送间隔为 %1 毫秒").arg(value));
}

void ClientWindow::handleAutoToggled(bool checked) {
    controller_.setAutoPayload(currentPayload());
    controller_.setAutoSending(checked);
    autoCheck_->setText(checked ? tr("自动发送中...") : tr("启用自动发送"));
    
    if (checked) {
        // 检查是否被服务器控制
        if (!serverControlled_->text().contains(tr("服务器控制"))) {
            intervalSpin_->setEnabled(true);
            serverControlled_->setText(tr("当前间隔由客户端控制"));
            serverControlled_->setStyleSheet("QLabel { color: blue; font-style: italic; }");
        }
        appendLog(tr("[自动] 已启用自动发送,间隔: %1 毫秒").arg(intervalSpin_->value()));
    } else {
        intervalSpin_->setEnabled(false);
        appendLog(tr("[自动] 已禁用自动发送"));
    }
}

void ClientWindow::handleStatisticsUpdated(int sent, int received) {
    sentLabel_->setText(tr("已发送: %1").arg(sent));
    receivedLabel_->setText(tr("已接收: %1").arg(received));
}

void ClientWindow::appendLog(const QString &line) {
    const QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    logView_->appendPlainText(QString("[%1] %2").arg(timestamp, line));
}

QByteArray ClientWindow::currentPayload() const {
    return payloadEdit_->toPlainText().toUtf8();
}
