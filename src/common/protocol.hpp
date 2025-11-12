#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <cstdint>
#include <optional>

namespace cs::protocol {

constexpr uint8_t kSof = 0xAA;
constexpr uint8_t kEof = 0x55;
constexpr uint8_t kDefaultVersion = 0x01;
constexpr uint16_t kMaxPayloadBytes = 4096;

enum class FrameError {
    None = 0,
    MissingSOF,
    LengthTooLarge,
    LengthMismatch,
    InvalidCRC,
    InvalidEOF,
    UnsupportedVersion,
};

struct Frame {
    uint8_t version = kDefaultVersion;
    QByteArray payload;
};

struct ParsedFrame {
    Frame frame;
    QByteArray rawBytes;
};

class ProtocolParser {
public:
    void append(const QByteArray &data);
    std::optional<ParsedFrame> nextFrame(FrameError *error = nullptr, QString *message = nullptr);
    void clear();

private:
    QByteArray buffer_;
};

QByteArray build_frame(uint8_t version, const QByteArray &payload);

}  // namespace cs::protocol
