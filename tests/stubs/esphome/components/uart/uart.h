#pragma once

#include <cstdint>
#include <deque>
#include <vector>

namespace esphome {
namespace uart {

class UARTDevice {
 public:
  std::deque<uint8_t> received;
  std::vector<std::vector<uint8_t>> transmitted;

  size_t available() const { return received.size(); }
  bool read_byte(uint8_t *byte) {
    if (received.empty()) return false;
    *byte = received.front();
    received.pop_front();
    return true;
  }
  void write_array(const uint8_t *data, size_t size) { transmitted.emplace_back(data, data + size); }
};
}  // namespace uart
}  // namespace esphome
