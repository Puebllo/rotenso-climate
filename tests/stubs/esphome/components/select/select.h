#pragma once

#include <string>

namespace esphome {
namespace select {
class Select {
 public:
  virtual ~Select() = default;
  std::string state;
  unsigned published_states{0};
  void publish_state(const std::string &value) { state = value; published_states++; }
  void make_test_call(const std::string &value) { this->control(value); }

 protected:
  virtual void control(const std::string &value) = 0;
};
}  // namespace select
}  // namespace esphome
