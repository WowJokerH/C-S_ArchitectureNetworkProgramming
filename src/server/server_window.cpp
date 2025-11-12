#include "server_window.hpp"

#include <QtCore/QDateTime>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QTableView>
#include <QtWidgets/QVBoxLayout>

namespace {

struct PayloadSummary {
    quint8 type = 0;
    quint16 msgId = 0;
    QByteArray content;
};

PayloadSummary summarizePayload(const QByteArray &payload) {
    PayloadSummary summary;
    if (payload.size() >= 3) {
        summary.type = static_cast<quint8>(payload.at(0));
        summary.msgId = (static_cast<quint8>(payload.at(1)) << 8) | static_cast<quint8>(payload.at(2));
        summary.content = payload.mid(3);
    } else {
        summary.content = payload;
    }
    return summary;
}

QString hexPreview(const QByteArray &payload, int maxBytes = 64) {
    const QByteArray slice = payload.left(maxBytes);
    QString hex = QString::fromLatin1(slice.toHex(' ').toUpper());
    if (payload.size() > maxBytes) {
        hex.append(QStringLiteral(" ..."));
    }
    return hex;
}

QString textPreview(const QByteArray &payload, int maxChars = 64) {
    QString text = QString::fromUtf8(payload);
    QString sanitized;
    sanitized.reserve(text.size());
    for (const auto &ch : text) {
        sanitized.append(ch.isPrint() ? ch : QLatin1Char('.'));
    }
    if (sanitized.size() > maxChars) {
        sanitized = sanitized.left(maxChars) + QStringLiteral("...");
    }
    return sanitized;
}

}  // namespace

ServerWindow::ServerWindow(QWidget *parent)
    : QMainWindow(parent),
      listener_(new Listener(this)),
      model_(new ConnectionModel(this)) {
    auto *central = new QWidget(this);
    setCentralWidget(central);

    // 连接视图 - 改进表格显示
    connectionView_ = new QTableView(central);
    connectionView_->setModel(model_);
    connectionView_->setSelectionBehavior(QTableView::SelectRows);
    connectionView_->setSelectionMode(QTableView::SingleSelection);
    connectionView_->setAlternatingRowColors(true);
    connectionView_->horizontalHeader()->setStretchLastSection(true);
    connectionView_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    connectionView_->setMinimumHeight(150);
    connectionView_->setStyleSheet(
        "QTableView { gridline-color: #d0d0d0; }"
        "QTableView::item:selected { background-color: #0078d7; color: white; }"
    );

    // 日志视图 - 改进显示
    logView_ = new QPlainTextEdit(central);
    logView_->setReadOnly(true);
    logView_->setMaximumBlockCount(1000);
    logView_->setMinimumHeight(150);
    logView_->setStyleSheet(
        "QPlainTextEdit { "
        "   background-color: #f5f5f5; "
        "   font-family: 'Consolas', 'Courier New', monospace; "
        "   font-size: 9pt; "
        "}"
    );

    // 端口设置
    portSpin_ = new QSpinBox(central);
    portSpin_->setRange(1024, 65535);
    portSpin_->setValue(8080);
    portSpin_->setMinimumWidth(120);

    startBtn_ = new QPushButton(tr("启动服务器"), central);
    startBtn_->setMinimumWidth(120);
    startBtn_->setStyleSheet("QPushButton { font-weight: bold; padding: 8px; }");

    statusLabel_ = new QLabel(tr("未运行"), central);
    statusIndicator_ = new QLabel(tr("●"), central);
    statusIndicator_->setStyleSheet("QLabel { color: red; font-size: 16pt; font-weight: bold; }");

    // 间隔控制
    intervalCheck_ = new QCheckBox(tr("强制客户端数据间隔"), central);
    intervalCheck_->setToolTip(tr("勾选后,服务器将通过响应包控制客户端发送数据的时间间隔"));
    
    intervalSpin_ = new QSpinBox(central);
    intervalSpin_->setRange(500, 60000);
    intervalSpin_->setSingleStep(500);
    intervalSpin_->setSuffix(tr(" 毫秒"));
    intervalSpin_->setValue(3000);
    intervalSpin_->setEnabled(false);
    intervalSpin_->setMinimumWidth(120);

    // 控制面板组 - 改进布局
    auto *controlGroup = new QGroupBox(tr("服务器控制面板"), central);
    auto *controlLayout = new QGridLayout(controlGroup);
    
    auto *portLabel = new QLabel(tr("监听端口:"), controlGroup);
    controlLayout->addWidget(portLabel, 0, 0);
    controlLayout->addWidget(portSpin_, 0, 1);
    controlLayout->addWidget(startBtn_, 0, 2, 1, 2);
    
    auto *statusLabel = new QLabel(tr("服务器状态:"), controlGroup);
    controlLayout->addWidget(statusLabel, 1, 0);
    controlLayout->addWidget(statusIndicator_, 1, 1);
    controlLayout->addWidget(statusLabel_, 1, 2, 1, 2);
    controlLayout->setColumnStretch(3, 1);

    // 间隔控制组 - 改进布局
    auto *intervalGroup = new QGroupBox(tr("数据间隔控制"), central);
    auto *intervalLayout = new QGridLayout(intervalGroup);
    intervalLayout->addWidget(intervalCheck_, 0, 0, 1, 2);
    intervalLayout->addWidget(new QLabel(tr("目标间隔:"), intervalGroup), 1, 0);
    intervalLayout->addWidget(intervalSpin_, 1, 1);
    intervalLayout->setColumnStretch(2, 1);

    // 主布局
    auto *layout = new QVBoxLayout;
    layout->addWidget(controlGroup);
    layout->addWidget(intervalGroup);
    layout->addWidget(new QLabel(tr("活动连接列表:"), central));
    layout->addWidget(connectionView_, 2);
    layout->addWidget(new QLabel(tr("运行日志:"), central));
    layout->addWidget(logView_, 3);
    layout->setSpacing(10);
    layout->setContentsMargins(10, 10, 10, 10);
    central->setLayout(layout);

    setWindowTitle(tr("C/S 服务器监控系统"));
    resize(1000, 700);

    connect(startBtn_, &QPushButton::clicked, this, &ServerWindow::handleStartStop);
    connect(intervalCheck_, &QCheckBox::toggled, this, &ServerWindow::updateIntervalSettings);
    connect(intervalSpin_, qOverload<int>(&QSpinBox::valueChanged), this, &ServerWindow::updateIntervalSettings);

    connect(listener_, &Listener::connectionUpdated, this, &ServerWindow::handleConnectionUpdated);
    connect(listener_, &Listener::connectionClosed, this, &ServerWindow::handleConnectionClosed);
    connect(listener_, &Listener::frameReceived, this, &ServerWindow::handleFrameReceived);
    connect(listener_, &Listener::invalidPacket, this, &ServerWindow::handleInvalidPacket);
    connect(listener_, &Listener::logMessage, this, &ServerWindow::handleLogMessage);
    connect(listener_, &Listener::listening, this, [this](quint16 port) {
        appendLog(tr("[系统] 服务器已启动,监听端口: %1").arg(port));
        refreshUiState();
    });
    connect(listener_, &Listener::stopped, this, [this]() {
        appendLog(tr("[系统] 服务器已停止"));
        refreshUiState();
    });
    
    connect(intervalCheck_, &QCheckBox::toggled, [this](bool checked) {
        intervalSpin_->setEnabled(checked);
        updateIntervalSettings();
    });

    appendLog(tr("[系统] 服务器监控系统已就绪"));
}

void ServerWindow::handleStartStop() {
    if (listener_->isListening()) {
        listener_->stop();
    } else {
        if (!listener_->start(static_cast<quint16>(portSpin_->value()))) {
            appendLog(tr("[错误] 启动监听失败,请检查端口是否被占用"));
        }
    }
    refreshUiState();
}

void ServerWindow::handleConnectionUpdated(const ConnectionRow &row) {
    model_->upsert(row);
}

void ServerWindow::handleConnectionClosed(const QString &id) {
    model_->markDisconnected(id);
}

void ServerWindow::handleFrameReceived(const QString &id, const QByteArray &payload) {
    const auto summary = summarizePayload(payload);
    appendLog(tr("[数据] 客户端 %1 | 帧长:%2字节 | 类型:0x%3 | 序号:%4 | 内容长度:%5 | HEX=%6 | 文本=%7")
                  .arg(id.left(8))
                  .arg(payload.size())
                  .arg(QString::number(summary.type, 16).rightJustified(2, QLatin1Char('0')))
                  .arg(summary.msgId)
                  .arg(summary.content.size())
                  .arg(hexPreview(summary.content, 32))
                  .arg(textPreview(summary.content, 32)));
}

void ServerWindow::handleInvalidPacket(const QString &id, const QString &reason) {
    appendLog(tr("[错误] 客户端 %1 发送非法数据包: %2").arg(id.left(8), reason));
}

void ServerWindow::handleLogMessage(const QString &text) {
    appendLog(text);
}

void ServerWindow::updateIntervalSettings() {
    if (intervalCheck_->isChecked()) {
        listener_->setForcedInterval(intervalSpin_->value());
        appendLog(tr("[配置] 已启用强制间隔控制: %1 毫秒").arg(intervalSpin_->value()));
    } else {
        listener_->setForcedInterval(std::nullopt);
        appendLog(tr("[配置] 已禁用强制间隔控制"));
    }
}

void ServerWindow::appendLog(const QString &line) {
    const QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    logView_->appendPlainText(QString("[%1] %2").arg(timestamp, line));
}

void ServerWindow::refreshUiState() {
    const bool running = listener_->isListening();
    startBtn_->setText(running ? tr("停止服务器") : tr("启动服务器"));
    statusLabel_->setText(running ? tr("运行中") : tr("未运行"));
    statusIndicator_->setStyleSheet(running ? 
        "QLabel { color: green; font-size: 16pt; font-weight: bold; }" :
        "QLabel { color: red; font-size: 16pt; font-weight: bold; }");
    portSpin_->setEnabled(!running);
}
