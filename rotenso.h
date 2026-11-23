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

 protected:
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;
  void update() override;

  void send_heartbeat();
  void send_control();
  void parse_uart_response();
  void process_rx_buffer();
  void handle_frame(const std::vector<uint8_t> &frame);

  struct PendingControl {
    bool has_mode{false};
    bool has_temperature{false};
    bool has_fan_mode{false};
    bool has_preset{false};
    climate::ClimateMode mode{climate::CLIMATE_MODE_OFF};
    float temperature{0};
    climate::ClimateFanMode fan_mode{climate::CLIMATE_FAN_AUTO};
    climate::ClimatePreset preset{climate::CLIMATE_PRESET_NONE};

    bool any() const { return has_mode || has_temperature || has_fan_mode || has_preset; }
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
  std::array<uint8_t, RotensoFrameBuilder::FRAME_LENGTH> control_frame_{};
  std::vector<uint8_t> rx_buffer_;
};

}  // namespace rotenso
}  // namespace esphome
