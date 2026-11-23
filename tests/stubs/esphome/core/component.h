#pragma once

#include <cstdint>

extern uint32_t fake_millis;
inline uint32_t millis() { return fake_millis; }

namespace esphome {
class PollingComponent {
 public:
  virtual ~PollingComponent() = default;
  virtual void setup() {}
  virtual void loop() {}
  virtual void update() {}
};
}  // namespace esphome
