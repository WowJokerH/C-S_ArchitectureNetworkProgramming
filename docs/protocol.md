# 自定义通信协议

## 1. 帧结构

| 字段           | 长度 (字节) | 描述 |
|----------------|------------|------|
| `SOF`          | 1          | 起始标记，固定 `0xAA` |
| `Version`      | 1          | 协议版本（初始 `0x01`），保留高 4 位做兼容 |
| `Length`       | 2          | Payload 字节数（不含帧头、CRC、EOF），大端 |
| `Payload`      | N          | 业务内容，长度由 `Length` 决定 |
| `CRC16`        | 2          | 对 `Version + Length + Payload` 计算的 CRC16-CCITT |
| `EOF`          | 1          | 结束标记，固定 `0x55` |

**最小帧长**：6 字节（空 payload）。**最大帧长**：64 KB（可在配置中限制）。

## 2. Payload 格式

基础结构：

| 字段         | 长度 | 说明 |
|--------------|------|------|
| `MsgType`    | 1    | 0x01=文本、0x02=二进制、0x10=命令等 |
| `MsgId`      | 2    | 序号，客户端自增，服务器回显 |
| `Body`       | N    | 数据内容，格式由 `MsgType` 决定 |

### 2.1 响应帧 Payload

| 字段              | 长度 | 说明 |
|-------------------|------|------|
| `RespCode`        | 1    | 0x00=成功，0x01=非法包，其他保留 |
| `ServerTimestamp` | 8    | 毫秒时间戳（uint64） |
| `CmdId`           | 1    | 0=无命令，1=设置发送间隔 |
| `CmdPayload`      | 可选 | 例如 `uint32 intervalMs` |

## 3. CRC16-CCITT 细节

**算法参数**：
- **多项式**：`0x1021` (CRC16-CCITT)
- **初始值**：`0xFFFF`
- **输入反转**：否（MSB first, left-shift）
- **输出反转**：否
- **最终异或值**：`0x0000`
- **计算范围**：`Version | Length(2) | Payload`

**实现代码（C++实际实现）**：

```cpp
// src/common/crc16.cpp
uint16_t crc16_ccitt(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}
```

**Python测试工具实现**：

```python
# test_invalid_packets.py
def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= (byte << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ 0x1021
            else:
                crc <<= 1
        crc &= 0xFFFF
    return crc
```

**注意事项**：
- 不要与CRC16-IBM混淆（IBM使用polynomial 0xA001，right-shift）
- CRC存储为大端字节序（MSB first）
- 验证CRC时，重新计算`Version|Length|Payload`的CRC并比较

## 4. 解析规则

1. 查找 `SOF`。若超时或缓冲溢出则丢弃缓存。
2. 读取 `Version`，校验是否支持。
3. 读取 `Length`（大端），若超出配置上限则报错。
4. 读取 `Length` 指定的 payload。
5. 读取 `CRC16`，与计算值比较，失败则报错。
6. 读取 `EOF`，必须为 `0x55`。
7. 解析 payload 并进入业务处理。

错误分类包括：`ERR_SOF_MISMATCH`、`ERR_LENGTH_TOO_BIG`、`ERR_UNDERRUN`、`ERR_CRC_FAIL`、`ERR_EOF_MISMATCH`。

## 5. 示例

客户端发送文本“HELLO”：

```
AA 01 00 08  01 00 01 48 45 4C 4C 4F  6E 2B 55
└┘ └┘ └─┘   └┘ └─┘ └───────┘  └──┘ └┘
SOF V  Len   Type MsgId Body      CRC  EOF
```

服务器响应（设置间隔为 5000 ms）：

```
AA 01 00 0C  00 00 00 00 00 01 97 2F  01 00 00 13 88  3B A1 55
```

- `RespCode=0x00`
- `ServerTimestamp=0x000000000001972F` (103215 ms)
- `CmdId=0x01`
- `Interval=0x00001388` (5000 ms)
