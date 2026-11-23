#pragma once

#include <cstdint>
#include <string>

#include "esphome/components/select/select.h"

namespace esphome {
namespace rotenso {

// The range/position byte and the movement flag are separate in AC responses.
struct SwingState {
  uint8_t value{0};
  bool moving{false};
};

bool swing_state_from_option(bool vertical, const std::string &option, SwingState &state);
const char *swing_option_from_state(bool vertical, const SwingState &state);
bool swing_states_match(bool vertical, const SwingState &left, const SwingState &right);

class RotensoClimate;

class RotensoSwingSelect : public select::Select {
 public:
  void set_parent(RotensoClimate *parent, bool vertical) {
    this->parent_ = parent;
    this->vertical_ = vertical;
  }
  void publish_swing_state(const SwingState &state);

 protected:
  void control(const std::string &value) override;

  RotensoClimate *parent_{nullptr};
  bool vertical_{false};
};

}  // namespace rotenso
}  // namespace esphome
