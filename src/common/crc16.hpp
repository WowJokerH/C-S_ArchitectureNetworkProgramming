#pragma once

#include <cstddef>
#include <cstdint>

namespace cs::protocol {

uint16_t crc16_ibm(const uint8_t *data, std::size_t size);

}  // namespace cs::protocol
