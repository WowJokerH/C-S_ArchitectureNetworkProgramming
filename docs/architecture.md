# 架构设计

## 1. 总体概览

- **拓扑**：标准 TCP C/S 模型。服务器常驻监听端口（默认 8080，可配置），客户端建立长连接实现双向通信。
- **跨平台**：C++17 + Qt 6（Qt Widgets/QtNetwork）统一提供 GUI 和网络封装，CMake 生成 Windows/Linux 双平台工程。
- **模块划分**
  1. **NetCore**：套接字封装、连接生命周期与事件派发。
  2. **Protocol**：帧打包/解包、CRC16验证、错误分类。
  3. **Business**：业务语义、响应构建、间隔调度策略。
  4. **UI/UX**：界面展示、交互、日志和状态指示。
  5. **Support**：配置、日志、诊断、测试夹具。

## 2. 服务器端设计

### 2.1 线程与事件

- 主线程运行 `QApplication`，负责 UI、日志显示。
- `Listener`（QTcpServer）绑定端口并监听连接。
- 每个新连接交由 `SessionWorker` 处理：
  - 一连接一 QThread，方便隔离阻塞操作，适合中小规模连接（实际实现）。
- 共享数据（活动连接表、运行时配置）通过 `std::shared_ptr<ServerRuntimeConfig>` 和原子操作保护。

### 2.2 会话处理流程

1. `readyRead` 事件触发，读取 socket 数据进入缓冲。
2. `ProtocolParser` 状态机查找 `0xAA` 起始标记，解析版本与长度。
3. 按长度提取 payload，计算 `CRC16-CCITT` 并与包内校验码比较。
4. 检查结束符 `0x55`，若不匹配则报错。
5. 合法帧进入业务处理，执行：
   - 记录 IP、端口、时间戳与 payload。
   - 发送 ACK 响应包。
   - 需要时附带 `CMD_SET_INTERVAL` 指令调整客户端发送周期。
6. 若解析失败，通过 `invalidPacket` 信号输出具体错误到UI日志。

### 2.3 活动连接管理

- `ConnectionModel`（继承QAbstractTableModel）管理活动连接表。
- 显示：连接ID、IP地址、端口、状态、最后活动时间、发送间隔。
- 会话线程通过 `connectionUpdated` 信号更新模型；UI通过QTableView展示。
- 服务器停止时，所有连接通过 `connectionClosed` 信号正确清理。

## 3. 客户端设计

### 3.1 UI 布局（已实现）

- **连接控制面板**：
  - 服务器地址、端口输入框
  - 连接/断开按钮
  - 状态指示器：●红色=未连接、●橙色=连接中、●绿色=已连接
  - 连接状态文本显示
  
- **数据发送面板**：
  - 多行文本输入框（支持UTF-8文本）
  - 立即发送按钮
  
- **自动发送设置**：
  - 启用/禁用自动发送复选框
  - 发送间隔调整（毫秒，默认3000ms）
  - 服务器控制提示标签（显示间隔是否被服务器强制控制）
  
- **通信统计**：
  - 已发送数据包计数
  - 已接收响应计数
  
- **日志窗口**：
  - 带时间戳的通信日志（精确到毫秒）
  - 分类标签：[系统][连接][发送][接收][错误][配置]
  - 等宽字体显示，最多保留1000条

### 3.2 交互流程（实际实现）

1. **连接建立**：
   - 用户输入地址端口并点击"连接"
   - `ClientController` 使用 `QTcpSocket` 发起连接
   - 连接成功后更新状态指示器为绿色，启用发送控件
   - 初始化统计计数器为0

2. **手动发送**：
   - 读取输入框内容，构建payload（消息类型0x01 + 消息ID + 内容）
   - 调用 `buildRequestPayload` 打包
   - 使用 `build_frame` 添加协议头和CRC16
   - 写入socket，发送计数+1

3. **自动发送**：
   - `QTimer` 按设定间隔触发
   - 默认3000ms，可通过UI调整
   - 每次发送后等待ACK响应（超时5秒）
   - 超时未收到响应则触发重连

4. **接收响应**：
   - `ProtocolParser` 解析接收到的数据帧
   - 提取响应码、时间戳、命令ID
   - 如包含 `CMD_SET_INTERVAL` (0x01)，更新定时器间隔
   - 显示"当前间隔由服务器控制"提示
   - 接收计数+1

5. **断线重连**：
   - 检测到断开连接时自动尝试重连
   - 3秒后重连（`reconnectTimer_`）
   - 显示重连倒计时日志

## 4. 共享协议与组件

- `src/common/protocol.hpp/cpp` 提供帧结构、CRC16-CCITT算法、错误枚举
- `src/common/crc16.hpp/cpp` 实现CRC16-CCITT（多项式0x1021，左移MSB-first）
- `src/common/logger.hpp/cpp` 提供日志功能
- 客户端与服务器共用这些组件，确保协议一致性
- 测试工具 `test_invalid_packets.py` 验证各种异常数据包的处理

## 5. 数据流与状态

1. **客户端 -> 服务器**：
   - UI输入 → ClientController::buildRequestPayload 
   - → protocol::build_frame (添加SOF/VERSION/LENGTH/CRC16/EOF)
   - → QTcpSocket::write → 网络传输
   - → 服务器Listener::handleNewConnection → SessionWorker
   - → ProtocolParser::nextFrame → 业务处理

2. **服务器 -> 客户端**：
   - SessionWorker::buildAckPayload (响应码+时间戳+命令)
   - → protocol::build_frame → QTcpSocket::write
   - → 客户端ProtocolParser → ClientController::handleAckPayload
   - → UI日志显示 / 间隔控制器更新

3. **状态同步**：
   - 服务器：`connectionUpdated`/`connectionClosed` 信号 → ConnectionModel → QTableView
   - 客户端：`statusChanged`/`statisticsUpdated` 信号 → ClientWindow → UI更新
   - 所有信号槽连接确保线程安全

## 6. 部署与运行（实际构建）

### 构建步骤
```bash
# 配置（需要Qt 6.x）
cmake -S . -B build -DCMAKE_PREFIX_PATH=<Qt路径>

# 编译
cmake --build build --config Release

# Windows下使用Ninja生成器
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=<Qt路径>
cmake --build build
```

### 可执行文件位置
- **服务器**: `build/src/server/server.exe` (Windows) 或 `build/src/server/server` (Linux)
- **客户端**: `build/src/client/client.exe` (Windows) 或 `build/src/client/client` (Linux)

### 测试工具
- `test_invalid_packets.py` - Python测试脚本（需要Python 3.x）
- `run_test.bat` - Windows批处理启动脚本
- `diagnose_crc.py` - CRC算法诊断工具

## 7. 关键特性

### 已实现功能
- ✅ TCP长连接通信（服务器支持多客户端）
- ✅ 自定义协议（SOF/VERSION/LENGTH/PAYLOAD/CRC16/EOF）
- ✅ CRC16-CCITT校验（多项式0x1021）
- ✅ 完整的错误检测和日志记录
- ✅ 服务器端间隔控制（强制客户端调整发送间隔）
- ✅ 客户端自动重连机制
- ✅ 实时通信统计（发送/接收计数）
- ✅ 专业UI界面（状态指示器、分类日志、表格视图）
- ✅ 线程安全的连接管理
- ✅ 异常数据包测试工具

### 技术亮点
- 使用Qt信号槽实现线程间通信，避免显式锁
- 每个客户端连接独立线程处理，互不干扰
- 状态机解析协议，支持流式数据处理
- 详细的错误分类（SOF/CRC/EOF/LENGTH/VERSION）
- 毫秒级精确时间戳日志
- 服务器优雅停止（正确断开所有客户端）

### 可扩展方向
- 支持 TLS/SSL 加密通信（使用QSslSocket）
- 添加用户认证和访问控制
- 数据持久化（连接历史、消息记录）
- 性能监控和报警机制
- RESTful API接口（用于外部系统集成）
- 支持更多payload类型和自定义命令
