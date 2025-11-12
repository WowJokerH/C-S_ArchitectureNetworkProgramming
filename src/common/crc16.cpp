#include "crc16.hpp"

namespace cs::protocol {

namespace {

constexpr uint16_t kPoly = 0x1021;
constexpr uint16_t kInit = 0xFFFF;

}  // namespace

uint16_t crc16_ibm(const uint8_t *data, std::size_t size) {
    uint16_t crc = kInit;
    for (std::size_t idx = 0; idx < size; ++idx) {
        const uint8_t byte = data[idx];
        crc ^= static_cast<uint16_t>(byte) << 8;
        for (int i = 0; i < 8; ++i) {
            const bool carry = (crc & 0x8000) != 0;
            crc <<= 1;
            if (carry) {
                crc ^= kPoly;
            }
        }
    }
    return crc;
}

}  // namespace cs::protocol
