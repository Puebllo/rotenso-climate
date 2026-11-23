#pragma once

#include <array>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "rotenso_frame_builder.h"

namespace esphome {
namespace rotenso {

class RotensoClimate : public climate::Climate, public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void set_vertical_swing_select(RotensoSwingSelect *select) {
    this->vertical_swing_select_ = select;
    select->set_parent(this, true);
  }
  void set_horizontal_swing_select(RotensoSwingSelect *select) {
    this->horizontal_swing_select_ = select;
    select->set_parent(this, false);
  }
  void set_swing_state(bool vertical, const SwingState &state);
  SwingState get_swing_state(bool vertical) const {
    return vertical ? this->vertical_swing_ : this->horizontal_swing_;
  }

 protected:
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;
  void update() override;

  void send_heartbeat();
  void send_control();
  void parse_uart_response();
  void process_rx_buffer();
  void handle_frame(const std::vector<uint8_t> &frame);
  SwingState get_desired_swing_state(bool vertical) const;

  struct PendingControl {
    bool has_mode{false};
    bool has_temperature{false};
    bool has_fan_mode{false};
    bool has_preset{false};
    bool has_vertical_swing{false};
    bool has_horizontal_swing{false};
    climate::ClimateMode mode{climate::CLIMATE_MODE_OFF};
    float temperature{0};
    climate::ClimateFanMode fan_mode{climate::CLIMATE_FAN_AUTO};
    climate::ClimatePreset preset{climate::CLIMATE_PRESET_NONE};
    SwingState vertical_swing;
    SwingState horizontal_swing;

    bool any() const {
      return has_mode || has_temperature || has_fan_mode || has_preset || has_vertical_swing || has_horizontal_swing;
    }
  };

  enum class Request : uint8_t { NONE, HEARTBEAT, CONTROL };

  uint32_t last_heartbeat_{0};
  uint32_t last_bus_activity_{0};
  uint32_t last_rx_byte_{0};
  uint32_t request_started_{0};
  bool have_state_{false};
  bool need_status_refresh_{true};
  bool force_heartbeat_{true};
  Request request_{Request::NONE};
  uint8_t control_retries_{0};
  PendingControl pending_control_;
  PendingControl in_flight_control_;
  SwingState vertical_swing_;
  SwingState horizontal_swing_;
  RotensoSwingSelect *vertical_swing_select_{nullptr};
  RotensoSwingSelect *horizontal_swing_select_{nullptr};
  std::array<uint8_t, RotensoFrameBuilder::FRAME_LENGTH> control_frame_{};
  std::vector<uint8_t> rx_buffer_;
};

}  // namespace rotenso
}  // namespace esphome
