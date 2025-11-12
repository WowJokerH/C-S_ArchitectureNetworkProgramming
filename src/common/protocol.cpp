#include "protocol.hpp"

#include "crc16.hpp"

#include <QtCore/QByteArray>

#include <cstddef>
namespace cs::protocol {

namespace {

constexpr int kFixedFrameBytes = 1 /*SOF*/ + 1 /*Version*/ + 2 /*Length*/ + 2 /*CRC*/ + 1 /*EOF*/;

void set_error(FrameError code, const QString &reason, FrameError *outCode, QString *outReason) {
    if (outCode) {
        *outCode = code;
    }
    if (outReason) {
        *outReason = reason;
    }
}

uint16_t read_u16(const QByteArray &bytes, int offset) {
    const uint8_t hi = static_cast<uint8_t>(bytes.at(offset));
    const uint8_t lo = static_cast<uint8_t>(bytes.at(offset + 1));
    return static_cast<uint16_t>((hi << 8) | lo);
}

}  // namespace

void ProtocolParser::append(const QByteArray &data) {
    buffer_.append(data);
}

void ProtocolParser::clear() {
    buffer_.clear();
}

std::optional<ParsedFrame> ProtocolParser::nextFrame(FrameError *error, QString *message) {
    if (error) {
        *error = FrameError::None;
    }
    if (message) {
        message->clear();
    }

    while (true) {
        const int sofIndex = buffer_.indexOf(char(kSof));
        if (sofIndex < 0) {
            if (!buffer_.isEmpty()) {
                buffer_.clear();
                set_error(FrameError::MissingSOF, QStringLiteral("SOF not found"), error, message);
            }
            return std::nullopt;
        }

        if (sofIndex > 0) {
            buffer_.remove(0, sofIndex);
        }

        if (buffer_.size() < kFixedFrameBytes) {
            return std::nullopt;
        }

        const uint8_t version = static_cast<uint8_t>(buffer_.at(1));
        if (version != kDefaultVersion) {
            buffer_.remove(0, 1);  // Skip unexpected version byte while retaining SOF search.
            set_error(FrameError::UnsupportedVersion, QStringLiteral("Unsupported version %1").arg(version), error, message);
            continue;
        }

        const uint16_t payloadLen = read_u16(buffer_, 2);
        if (payloadLen > kMaxPayloadBytes) {
            buffer_.remove(0, 1);
            set_error(FrameError::LengthTooLarge, QStringLiteral("Payload %1 exceeds limit").arg(payloadLen), error, message);
            continue;
        }

        const int frameSize = 1 + 1 + 2 + payloadLen + 2 + 1;
        if (buffer_.size() < frameSize) {
            return std::nullopt;
        }

        const uint8_t eof = static_cast<uint8_t>(buffer_.at(frameSize - 1));
        if (eof != kEof) {
            buffer_.remove(0, 1);
            set_error(FrameError::InvalidEOF, QStringLiteral("Invalid EOF 0x%1").arg(QString::number(eof, 16)), error, message);
            continue;
        }

        const QByteArray headerPlusPayload = buffer_.mid(1, 1 + 2 + payloadLen);
        const auto crcProvided = read_u16(buffer_, 1 + 1 + 2 + payloadLen);
        const auto *ptr = reinterpret_cast<const uint8_t *>(headerPlusPayload.constData());
        const uint16_t crcCalculated = crc16_ibm(ptr, static_cast<std::size_t>(headerPlusPayload.size()));
        if (crcCalculated != crcProvided) {
            buffer_.remove(0, 1);
            set_error(FrameError::InvalidCRC,
                      QStringLiteral("CRC mismatch calc=0x%1 recv=0x%2")
                          .arg(QString::number(crcCalculated, 16))
                          .arg(QString::number(crcProvided, 16)),
                      error, message);
            continue;
        }

        ParsedFrame parsed;
        parsed.frame.version = version;
        parsed.frame.payload = buffer_.mid(4, payloadLen);
        parsed.rawBytes = buffer_.left(frameSize);
        buffer_.remove(0, frameSize);
        return parsed;
    }
}

QByteArray build_frame(uint8_t version, const QByteArray &payload) {
    QByteArray frame;
    const uint16_t payloadLen = static_cast<uint16_t>(payload.size());
    const int frameSize = 1 + 1 + 2 + payloadLen + 2 + 1;
    frame.reserve(frameSize);
    frame.append(char(kSof));
    QByteArray headerPayload;
    headerPayload.append(char(version));
    headerPayload.append(char((payloadLen >> 8) & 0xFF));
    headerPayload.append(char(payloadLen & 0xFF));
    headerPayload.append(payload);

    const auto *ptr = reinterpret_cast<const uint8_t *>(headerPayload.constData());
    const auto crc = crc16_ibm(ptr, static_cast<std::size_t>(headerPayload.size()));

    frame.append(headerPayload);
    frame.append(char((crc >> 8) & 0xFF));
    frame.append(char(crc & 0xFF));
    frame.append(char(kEof));
    return frame;
}

}  // namespace cs::protocol
