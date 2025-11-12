@echo off
chcp 65001 >nul
echo ========================================
echo Server Invalid Packet Test Tool
echo ========================================
echo.
echo Usage:
echo 1. Start server (server.exe)
echo 2. Click "Start Server" button
echo 3. Run this script
echo.
echo Tests include:
echo   - Wrong SOF/EOF markers
echo   - Wrong CRC16 checksum
echo   - Truncated frames
echo   - Wrong length field
echo   - Empty payload
echo   - Random bytes
echo   - Oversized payload
echo   - Wrong protocol version
echo   - Valid packet (control group)
echo.
pause

python test_invalid_packets.py

pause
