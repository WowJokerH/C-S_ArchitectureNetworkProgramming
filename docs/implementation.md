# 实现方案

## 1. 工程结构（实际实现）

```
CMakeLists.txt                    # 主CMake配置
src/
  common/                         # 共享组件
    protocol.hpp / .cpp           # 帧打包、解析、CRC16
    crc16.hpp / .cpp              # CRC16-CCITT实现
    logger.hpp / .cpp             # 日志功能
    CMakeLists.txt
  server/                         # 服务器端
    main.cpp                      # 程序入口
    server_window.hpp / .cpp      # 主窗口UI
    listener.hpp / .cpp           # 监听器（QTcpServer）
    session_worker.hpp / .cpp     # 会话工作线程
    connection_model.hpp / .cpp   # 连接表格模型
    server_runtime.hpp            # 运行时配置
    CMakeLists.txt
  client/                         # 客户端
    main.cpp                      # 程序入口
    client_window.hpp / .cpp      # 主窗口UI
    client_controller.hpp / .cpp  # 控制器（socket管理）
    CMakeLists.txt
build/                            # 构建输出目录
  src/
    server/server.exe             # 服务器可执行文件
    client/client.exe             # 客户端可执行文件
docs/                             # 文档
test_invalid_packets.py           # Python测试工具
run_test.bat                      # 测试启动脚本
diagnose_crc.py                   # CRC诊断工具
```

## 2. 核心类说明（实际实现）

### 2.1 `ProtocolParser` (src/common/protocol.cpp)

- **职责**：增量解析字节流，返回完整帧或错误
- **关键字段**：
  ```cpp
  QByteArray buffer_;  // 接收缓冲区
  ```
- **关键方法**：
  ```cpp
  std::optional<ParsedFrame> nextFrame(FrameError *error, QString *message);
  void append(const QByteArray &data);  // 添加接收数据
  QByteArray build_frame(uint8_t version, const QByteArray &payload);  // 构建帧
  ```
- **错误类型**：
  ```cpp
  enum class FrameError {
      None, MissingSOF, LengthTooLarge, LengthMismatch, 
      InvalidCRC, InvalidEOF, UnsupportedVersion
  };
  ```

### 2.2 `SessionWorker` (src/server/session_worker.cpp)

- **位于独立QThread**，管理单个客户端连接
- **信号**：
  ```cpp
  void connectionUpdated(ConnectionRow row);     // 连接状态更新
  void frameReceived(QString connectionId, QByteArray payload);  // 收到数据
  void invalidPacket(QString connectionId, QString reason);      // 非法包
  void finished(QString connectionId);           // 连接结束
  ```
- **关键方法**：
  ```cpp
  void start();           // 启动会话
  void stop();            // 停止会话
  void onReadyRead();     // 处理接收数据
  void onDisconnected();  // 处理断开连接
  void sendAck(bool success);  // 发送ACK响应
  ```
- **处理流程**：
  ```cpp
  void SessionWorker::onReadyRead() {
      parser_->append(socket_->readAll());
      while (true) {
          FrameError error;
          QString reason;
          auto frame = parser_->nextFrame(&error, &reason);
          if (!frame) {
              if (error != FrameError::None)
                  emit invalidPacket(connectionId_, reason);
              break;
          }
          emit frameReceived(connectionId_, frame->frame.payload);
          sendAck(true);
      }
  }
  ```

### 2.3 `Listener` (src/server/listener.cpp)

- **继承QObject**，管理QTcpServer和所有SessionWorker
- **关键成员**：
  ```cpp
  QTcpServer *server_;
  std::unordered_map<QString, SessionWorker*> sessions_;
  std::unordered_map<QString, QThread*> threads_;
  std::shared_ptr<ServerRuntimeConfig> runtimeConfig_;  // 共享配置
  ```
- **信号**：
  ```cpp
  void listening(quint16 port);
  void stopped();
  void connectionUpdated(const ConnectionRow &row);
  void connectionClosed(const QString &id);
  void frameReceived(const QString &id, QByteArray payload);
  void invalidPacket(const QString &id, QString reason);
  ```
- **关键方法**：
  ```cpp
  bool start(quint16 port);                      // 启动监听
  void stop();                                   // 停止并清理所有连接
  void setForcedInterval(std::optional<int>);    // 设置强制间隔
  ```

### 2.4 客户端 `ClientController` (src/client/client_controller.cpp)

- **管理客户端连接和通信逻辑**
- **关键成员**：
  ```cpp
  QTcpSocket socket_;
  QTimer autoTimer_;         // 自动发送定时器
  QTimer reconnectTimer_;    // 重连定时器
  QTimer ackTimer_;          // ACK超时定时器
  ProtocolParser parser_;
  int sentCount_, receivedCount_;  // 统计
  ```
- **信号**：
  ```cpp
  void statusChanged(QString status);
  void statisticsUpdated(int sent, int received);
  void intervalUpdated(int milliseconds);
  ```
- **关键方法**：
  ```cpp
  void connectToHost(QString host, quint16 port);
  void disconnectFromHost();
  void sendPayload(const QByteArray &payload);
  void setAutoInterval(int ms);
  void setAutoSending(bool enabled);
  ```

## 3. UI 实现（实际代码）

### 3.1 服务器窗口 (ServerWindow)

**控件组织**：
- `QSpinBox *portSpin_` - 端口配置
- `QPushButton *startBtn_` - 启动/停止按钮
- `QLabel *statusIndicator_` - 状态指示器（●）
- `QCheckBox *intervalCheck_` - 启用间隔控制
- `QSpinBox *intervalSpin_` - 间隔时间设置
- `QTableView *connectionView_` - 连接列表（使用ConnectionModel）
- `QPlainTextEdit *logView_` - 日志显示

**信号连接**：
```cpp
connect(startBtn_, &QPushButton::clicked, this, &ServerWindow::handleStartStop);
connect(intervalCheck_, &QCheckBox::toggled, this, &ServerWindow::updateIntervalSettings);
connect(listener_, &Listener::frameReceived, this, &ServerWindow::handleFrameReceived);
connect(listener_, &Listener::invalidPacket, this, &ServerWindow::handleInvalidPacket);
```

**日志格式**：
```cpp
void ServerWindow::appendLog(const QString &line) {
    const QString timestamp = QDateTime::currentDateTime()
        .toString("yyyy-MM-dd hh:mm:ss.zzz");
    logView_->appendPlainText(QString("[%1] %2").arg(timestamp, line));
}
```

### 3.2 客户端窗口 (ClientWindow)

**控件组织**：
- `QLineEdit *hostEdit_` - 服务器地址
- `QSpinBox *portSpin_` - 端口
- `QPushButton *connectBtn_` - 连接/断开
- `QLabel *statusIndicator_` - 状态指示器（●）
- `QPlainTextEdit *payloadEdit_` - 数据输入
- `QPushButton *sendBtn_` - 发送按钮
- `QCheckBox *autoCheck_` - 自动发送开关
- `QSpinBox *intervalSpin_` - 间隔设置
- `QLabel *serverControlled_` - 服务器控制提示
- `QLabel *sentLabel_, *receivedLabel_` - 统计显示
- `QPlainTextEdit *logView_` - 日志

**信号连接**：
```cpp
connect(connectBtn_, &QPushButton::clicked, this, &ClientWindow::handleConnectToggle);
connect(sendBtn_, &QPushButton::clicked, this, &ClientWindow::handleSendClicked);
connect(&controller_, &ClientController::statusChanged, 
        this, &ClientWindow::handleStatusChanged);
connect(&controller_, &ClientController::statisticsUpdated,
        this, &ClientWindow::handleStatisticsUpdated);
```

**UI状态管理**：
```cpp
void ClientWindow::handleStatusChanged(const QString &status) {
    const bool connected = (status == tr("已连接"));
    sendBtn_->setEnabled(connected);
    autoCheck_->setEnabled(connected);
    // 更新状态指示器颜色
    statusIndicator_->setStyleSheet(connected ? 
        "QLabel { color: green; ... }" : 
        "QLabel { color: red; ... }");
}
```

## 4. 配置与命令行（实际实现）

当前版本采用**硬编码默认值**，未实现外部配置文件。

**默认参数**：
- 服务器端口：8080
- 自动发送间隔：1000ms
- 日志级别：所有级别启用
- 协议版本：0x01
- CRC算法：CRC16-CCITT (polynomial 0x1021)

**未实现的功能**：
- ❌ YAML/JSON配置文件
- ❌ 命令行参数解析
- ❌ 动态日志级别调整

**可能的扩展方向**（未实现）：
```cpp
// 示例：如果未来添加配置文件支持
// QSettings settings("config.ini", QSettings::IniFormat);
// int port = settings.value("server/port", 8080).toInt();
```

## 5. 测试与验证（实际工具）

### 5.1 测试工具

**test_invalid_packets.py**：
- 功能：测试服务器对无效数据包的处理能力
- 测试用例：
  1. Invalid SOF (错误起始符)
  2. Invalid EOF (错误结束符)
  3. Invalid CRC (错误校验和)
  4. Wrong version (错误版本号)
  5. Length too short (长度字段过小)
  6. Length too long (长度字段过大)
  7. Incomplete frame (不完整数据帧)
  8. Extra bytes (额外字节)
  9. Missing CRC (缺少校验和)
  10. Valid packet (正常数据包)

**测试结果**：
```
Test 1 (Invalid SOF): Server rejected ✓
Test 2 (Invalid EOF): Server rejected ✓
Test 3 (Invalid CRC): Server rejected ✓
Test 4 (Wrong version): Server rejected ✓
Test 5 (Length too short): Server rejected ✓
Test 6 (Length too long): Server rejected ✓
Test 7 (Incomplete frame): Server rejected ✓
Test 8 (Extra bytes): Server rejected ✓
Test 9 (Missing CRC): Server rejected ✓
Test 10 (Valid packet): Server accepted and responded (34 bytes) ✓
```

**diagnose_crc.py**：
- 功能：分析和调试CRC校验问题
- 用途：验证Python测试工具与C++服务器使用相同的CRC16-CCITT算法

### 5.2 单元测试（未实现）

当前版本未包含C++单元测试框架，可能的扩展：
```cpp
// 示例：使用Qt Test框架
// TEST(ProtocolParser, ValidFrame) {
//     QByteArray frame = ...;
//     ProtocolParser parser;
//     EXPECT_TRUE(parser.feed(frame));
// }
```

### 5.3 集成测试

**手动测试流程**：
1. 启动服务器（默认端口8080）
2. 运行 `python test_invalid_packets.py` 验证协议健壮性
3. 启动客户端，测试连接/断开/重连
4. 测试服务器停止按钮功能（应断开所有连接）
5. 验证自动发送功能和间隔控制

## 6. 异常处理与重连（实际实现）

### 6.1 客户端重连机制

**实现位置**：`client_controller.cpp`

```cpp
void ClientController::handleDisconnected() {
    if (shouldReconnect_) {
        reconnectTimer_.start(3000);  // 3秒后重连
        emit statusChanged(tr("连接断开，3秒后重连..."));
    }
}
```

**特性**：
- 自动重连间隔：3秒（固定值，未实现指数退避）
- 重连条件：非用户主动断开时触发
- 状态反馈：通过statusChanged信号通知UI

### 6.2 服务器异常处理

**实现位置**：`session_worker.cpp`

```cpp
void SessionWorker::handleSocketError(QAbstractSocket::SocketError error) {
    Logger::error(QString("会话 %1 错误: %2")
        .arg(sessionId_).arg(socket_->errorString()));
    disconnect();
}
```

**特性**：
- 捕获套接字错误（QAbstractSocket::SocketError）
- 记录详细错误日志
- 自动断开有问题的连接
- 防止finished()信号重复发射（finished_标志）

### 6.3 日志系统

**实现位置**：`logger.cpp`

```cpp
void Logger::info(const QString &msg);
void Logger::warning(const QString &msg);
void Logger::error(const QString &msg);
```

**特性**：
- 三级日志：INFO/WARNING/ERROR
- 包含时间戳（毫秒精度）
- 控制台和UI双输出
- 未实现日志级别过滤（所有日志都显示）

## 7. 构建与打包（实际流程）

### 7.1 构建系统

**CMake配置**：
```bash
# Windows (使用Ninja生成器)
cmake -S . -B build -G Ninja
cmake --build build
```

**输出文件**：
- `build/src/server/server.exe`
- `build/src/client/client.exe`

### 7.2 运行脚本

**run_test.bat**（Windows）：
```batch
@echo off
echo Starting server with CRC16-CCITT...
start /B build\src\server\server.exe
timeout /t 2
echo Starting client...
start /B build\src\client\client.exe
```

### 7.3 打包（未实现）

当前版本未包含部署工具，可能的扩展：
- Windows：使用 `windeployqt` 打包Qt依赖
- Linux：创建AppImage或.deb包
- macOS：生成.dmg文件

**示例命令**（未实现）：
```bash
# Windows
windeployqt build/src/server/server.exe
# Linux
linuxdeploy --appdir=AppDir --executable=server
```
