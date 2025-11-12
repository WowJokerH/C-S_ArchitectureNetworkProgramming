#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
简单的CRC16-IBM诊断工具
验证Python和C++服务器的CRC计算是否一致
"""

import struct

def crc16_ibm_lsb_first(data):
    """CRC16-IBM (右移版本, LSB-first, 用于串口通信)"""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF

def crc16_ccitt_msb_first(data):
    """CRC16-CCITT (左移版本, MSB-first, 多项式0x1021)"""
    crc = 0xFFFF
    for byte in data:
        crc ^= (byte << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ 0x1021
            else:
                crc <<= 1
    return crc & 0xFFFF

print("=" * 60)
print("CRC16 算法诊断工具")
print("=" * 60)

# 测试数据: VERSION(1) + LENGTH(2) + PAYLOAD(5)
test_payload = b"Hello"
test_data = bytearray()
test_data.append(0x01)  # VERSION
test_data.extend(struct.pack('>H', len(test_payload)))  # LENGTH (大端序)
test_data.extend(test_payload)  # PAYLOAD

print(f"\n测试数据 (VERSION + LENGTH + PAYLOAD):")
print(f"  HEX: {' '.join(f'{b:02X}' for b in test_data)}")
print(f"  长度: {len(test_data)} 字节")

print(f"\nCRC16计算结果:")
crc_ibm = crc16_ibm_lsb_first(test_data)
crc_ccitt = crc16_ccitt_msb_first(test_data)

print(f"  CRC16-IBM  (LSB-first, 0xA001): 0x{crc_ibm:04X}")
print(f"  CRC16-CCITT (MSB-first, 0x1021): 0x{crc_ccitt:04X}")

print(f"\n完整数据帧 (使用CRC16-IBM):")
frame_ibm = bytearray()
frame_ibm.append(0xAA)  # SOF
frame_ibm.extend(test_data)  # VERSION + LENGTH + PAYLOAD
frame_ibm.extend(struct.pack('>H', crc_ibm))  # CRC (大端序)
frame_ibm.append(0x55)  # EOF
print(f"  {' '.join(f'{b:02X}' for b in frame_ibm)}")

print(f"\n完整数据帧 (使用CRC16-CCITT):")
frame_ccitt = bytearray()
frame_ccitt.append(0xAA)  # SOF
frame_ccitt.extend(test_data)  # VERSION + LENGTH + PAYLOAD
frame_ccitt.extend(struct.pack('>H', crc_ccitt))  # CRC (大端序)
frame_ccitt.append(0x55)  # EOF
print(f"  {' '.join(f'{b:02X}' for b in frame_ccitt)}")

print("\n" + "=" * 60)
print("对照检查:")
print("=" * 60)
print("请查看C++服务器代码中的CRC算法实现")
print("src/common/crc16.cpp 中使用的是哪种算法?")
print("\n如果服务器使用:")
print(f"  • CRC16-IBM (右移):  期望CRC = 0x{crc_ibm:04X}")
print(f"  • CRC16-CCITT (左移): 期望CRC = 0x{crc_ccitt:04X}")
