#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
测试服务器异常数据包处理能力
发送各种格式错误的数据包，验证服务器能否正确识别并拒绝

协议格式: [SOF(0xAA)] [VERSION(1)] [LENGTH(2,大端)] [PAYLOAD(n)] [CRC16(2,大端)] [EOF(0x55)]
"""

import socket
import struct
import time
import sys

def crc16_ibm(data):
    """计算CRC16-CCITT校验码（与C++服务器端一致）
    多项式: 0x1021 (MSB-first, 左移)
    初始值: 0xFFFF
    """
    crc = 0xFFFF
    for byte in data:
        crc ^= (byte << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ 0x1021
            else:
                crc <<= 1
    return crc & 0xFFFF

def create_valid_frame(payload):
    """创建一个合法的数据帧
    格式: [SOF(1)] [VERSION(1)] [LENGTH(2)] [PAYLOAD(n)] [CRC16(2)] [EOF(1)]
    """
    SOF = 0xAA
    EOF = 0x55
    VERSION = 0x01
    
    # 构建帧
    frame = bytearray()
    frame.append(SOF)
    
    # 构建需要校验的部分: VERSION + LENGTH + PAYLOAD
    header_payload = bytearray()
    header_payload.append(VERSION)
    
    # 长度（大端序，16位）
    length = len(payload)
    header_payload.extend(struct.pack('>H', length))
    
    # 负载数据
    header_payload.extend(payload)
    
    # 计算CRC16
    crc = crc16_ibm(header_payload)
    
    # 组装完整帧
    frame.extend(header_payload)
    frame.extend(struct.pack('>H', crc))  # CRC16大端序
    frame.append(EOF)
    
    return bytes(frame)

def test_case_1_wrong_start_marker(sock):
    """测试用例1: 错误的起始标记"""
    print("\n[测试1] 发送错误的起始标记 (0xBB 而非 0xAA)...")
    payload = b"Hello Server"
    frame = create_valid_frame(payload)
    frame = bytearray(frame)
    frame[0] = 0xBB  # 修改起始标记
    sock.sendall(bytes(frame))
    time.sleep(0.5)

def test_case_2_wrong_end_marker(sock):
    """测试用例2: 错误的结束标记"""
    print("\n[测试2] 发送错误的结束标记 (0x66 而非 0x55)...")
    payload = b"Test End Marker"
    frame = create_valid_frame(payload)
    frame = bytearray(frame)
    frame[-1] = 0x66  # 修改结束标记
    sock.sendall(bytes(frame))
    time.sleep(0.5)

def test_case_3_wrong_crc(sock):
    """测试用例3: 错误的CRC校验码"""
    print("\n[测试3] 发送错误的CRC校验码...")
    payload = b"Wrong CRC Test"
    frame = create_valid_frame(payload)
    frame = bytearray(frame)
    # 修改CRC（倒数第3-2字节）
    wrong_crc = 0x1234
    struct.pack_into('>H', frame, len(frame) - 3, wrong_crc)
    sock.sendall(bytes(frame))
    time.sleep(0.5)

def test_case_4_truncated_frame(sock):
    """测试用例4: 截断的数据帧（不完整）"""
    print("\n[测试4] 发送截断的数据帧...")
    payload = b"Truncated Frame"
    frame = create_valid_frame(payload)
    # 只发送一半
    sock.sendall(frame[:len(frame)//2])
    time.sleep(0.5)

def test_case_5_wrong_length(sock):
    """测试用例5: 错误的长度字段"""
    print("\n[测试5] 发送错误的长度字段...")
    payload = b"Wrong Length"
    frame = create_valid_frame(payload)
    frame = bytearray(frame)
    # 修改长度字段（字节2-3，VERSION后面）
    wrong_length = len(payload) + 100
    struct.pack_into('>H', frame, 2, wrong_length)
    sock.sendall(bytes(frame))
    time.sleep(0.5)

def test_case_6_empty_payload(sock):
    """测试用例6: 空负载"""
    print("\n[测试6] 发送空负载...")
    payload = b""
    frame = create_valid_frame(payload)
    sock.sendall(frame)
    time.sleep(0.5)

def test_case_7_random_bytes(sock):
    """测试用例7: 完全随机的字节流"""
    print("\n[测试7] 发送随机字节流...")
    import random
    random_data = bytes([random.randint(0, 255) for _ in range(50)])
    sock.sendall(random_data)
    time.sleep(0.5)

def test_case_8_valid_frame(sock):
    """测试用例8: 发送一个合法的数据包（对照组）"""
    print("\n[测试8] 发送合法数据包（对照组）...")
    # 构建合法的请求负载（客户端消息格式）
    payload = bytearray()
    payload.append(0x01)  # 消息类型
    payload.extend(struct.pack('>H', 1234))  # 消息ID（大端序16位）
    payload.extend(b"This is a VALID packet from Python test")  # 消息内容
    
    frame = create_valid_frame(bytes(payload))
    
    # 打印完整帧的结构（调试用）
    print(f"  帧结构分析:")
    print(f"    SOF: 0x{frame[0]:02X}")
    print(f"    VERSION: 0x{frame[1]:02X}")
    print(f"    LENGTH: {struct.unpack('>H', frame[2:4])[0]} (0x{frame[2]:02X}{frame[3]:02X})")
    payload_len = struct.unpack('>H', frame[2:4])[0]
    print(f"    PAYLOAD: {payload_len} bytes")
    crc_offset = 1 + 1 + 2 + payload_len
    print(f"    CRC16: 0x{frame[crc_offset]:02X}{frame[crc_offset+1]:02X}")
    print(f"    EOF: 0x{frame[-1]:02X}")
    
    # 验证CRC计算
    header_payload = frame[1:crc_offset]
    calc_crc = crc16_ibm(header_payload)
    recv_crc = struct.unpack('>H', frame[crc_offset:crc_offset+2])[0]
    print(f"    CRC验证: 计算值=0x{calc_crc:04X}, 帧中值=0x{recv_crc:04X} {'✓' if calc_crc == recv_crc else '✗'}")
    
    # 打印帧的16进制表示
    hex_str = ' '.join(f'{b:02X}' for b in frame[:30])
    if len(frame) > 30:
        hex_str += ' ...'
    print(f"  发送帧: {hex_str} (共{len(frame)}字节)")
    
    sock.sendall(frame)
    time.sleep(0.5)
    
    # 尝试接收响应
    try:
        sock.settimeout(2.0)
        response = sock.recv(1024)
        if response:
            print(f"✓ 收到服务器响应: {len(response)} 字节")
            hex_resp = ' '.join(f'{b:02X}' for b in response[:20])
            if len(response) > 20:
                hex_resp += ' ...'
            print(f"  响应帧: {hex_resp}")
        else:
            print("✗ 未收到响应")
    except socket.timeout:
        print("✗ 接收响应超时（检查服务器日志是否收到数据）")
    finally:
        sock.settimeout(None)

def test_case_9_oversized_payload(sock):
    """测试用例9: 超大负载"""
    print("\n[测试9] 发送超大负载 (10KB)...")
    large_payload = b"X" * 10240
    frame = create_valid_frame(large_payload)
    sock.sendall(frame)
    time.sleep(0.5)

def test_case_10_wrong_version(sock):
    """测试用例10: 错误的协议版本"""
    print("\n[测试10] 发送错误的协议版本 (0xFF 而非 0x01)...")
    payload = b"Wrong Version"
    frame = create_valid_frame(payload)
    frame = bytearray(frame)
    frame[1] = 0xFF  # 修改版本号
    sock.sendall(bytes(frame))
    time.sleep(0.5)

def main():
    HOST = '127.0.0.1'
    PORT = 8080
    
    print("=" * 60)
    print("服务器异常数据包测试工具")
    print("=" * 60)
    print(f"目标服务器: {HOST}:{PORT}")
    
    # CRC算法验证
    print("\n[自检] 验证CRC16-IBM算法...")
    test_data = b"\x01\x00\x05Hello"  # VERSION + LENGTH + PAYLOAD
    test_crc = crc16_ibm(test_data)
    print(f"  测试数据: {' '.join(f'{b:02X}' for b in test_data)}")
    print(f"  CRC16结果: 0x{test_crc:04X}")
    print(f"  算法验证: ✓ (如果服务器日志显示不同CRC值,请检查算法一致性)")
    
    print("\n请确保服务器已启动，然后按Enter键开始测试...")
    input()
    
    try:
        # 创建TCP连接
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((HOST, PORT))
        print(f"\n✓ 已连接到服务器 {HOST}:{PORT}")
        
        # 运行所有测试用例
        test_cases = [
            test_case_1_wrong_start_marker,
            test_case_2_wrong_end_marker,
            test_case_3_wrong_crc,
            test_case_4_truncated_frame,
            test_case_5_wrong_length,
            test_case_6_empty_payload,
            test_case_7_random_bytes,
            test_case_8_valid_frame,
            test_case_9_oversized_payload,
            test_case_10_wrong_version,
        ]
        
        for i, test_func in enumerate(test_cases, 1):
            try:
                test_func(sock)
            except Exception as e:
                print(f"✗ 测试失败: {e}")
            time.sleep(0.5)
        
        print("\n" + "=" * 60)
        print("测试完成！")
        print("=" * 60)
        print("\n请检查服务器日志，应该能看到:")
        print("  • 对非法数据包的错误提示")
        print("  • 对合法数据包的正常处理")
        print("  • 连接保持活跃（没有因异常数据而断开）")
        
        # 保持连接以便观察
        print("\n按Enter键关闭连接...")
        input()
        
    except ConnectionRefusedError:
        print(f"\n✗ 无法连接到服务器 {HOST}:{PORT}")
        print("请确保服务器正在运行！")
    except Exception as e:
        print(f"\n✗ 发生错误: {e}")
    finally:
        if 'sock' in locals():
            sock.close()
            print("连接已关闭")

if __name__ == '__main__':
    main()
