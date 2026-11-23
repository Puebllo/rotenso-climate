#include "rotenso_swing.h"
#include "rotenso.h"
#include "esphome/core/log.h"

namespace esphome {
namespace rotenso {

static const char *const TAG = "rotenso.swing";
static const char *const POSITION_OPTIONS[] = {
    "Stopped", "Position 1", "Position 2", "Position 3", "Position 4", "Position 5"};

bool swing_state_from_option(bool vertical, const std::string &option, SwingState &state) {
  for (uint8_t position = 0; position <= 5; position++) {
    if (option == POSITION_OPTIONS[position]) {
      // Fixed positions use wire values 1-5 on both axes.
      state.value = position;
      state.moving = false;
      return true;
    }
  }
  state.moving = true;
  if (option == "Swing 1-5") state.value = 0x08;
  else if (option == "Swing 1-3") state.value = vertical ? 0x18 : 0x10;
  else if (option == "Swing 3-5") state.value = vertical ? 0x10 : 0x20;
  else if (!vertical && option == "Swing 2-4") state.value = 0x18;
  else return false;
  return true;
}

const char *swing_option_from_state(bool vertical, const SwingState &state) {
  if (!state.moving) {
    uint8_t position = state.value & 0x07;
    if (position > 5) return "Unknown";
    return POSITION_OPTIONS[position];
  }
  switch (state.value & (vertical ? 0x18 : 0x38)) {
    case 0x08: return "Swing 1-5";
    case 0x10: return vertical ? "Swing 3-5" : "Swing 1-3";
    case 0x18: return vertical ? "Swing 1-3" : "Swing 2-4";
    case 0x20: return vertical ? "Unknown" : "Swing 3-5";
    default: return "Unknown";
  }
}

bool swing_states_match(bool vertical, const SwingState &left, const SwingState &right) {
  if (left.moving != right.moving) return false;
  const uint8_t mask = left.moving ? (vertical ? 0x18 : 0x38) : 0x07;
  return (left.value & mask) == (right.value & mask);
}

void RotensoSwingSelect::publish_swing_state(const SwingState &state) {
  this->publish_state(swing_option_from_state(this->vertical_, state));
}

void RotensoSwingSelect::control(const std::string &value) {
  SwingState state;
  if (this->parent_ == nullptr) return;
  if (!swing_state_from_option(this->vertical_, value, state)) {
    ESP_LOGW(TAG, "Cannot select '%s'; it only indicates an unrecognized AC state", value.c_str());
    this->publish_swing_state(this->parent_->get_swing_state(this->vertical_));
    return;
  }
  this->parent_->set_swing_state(this->vertical_, state);
}

}  // namespace rotenso
}  // namespace esphome
