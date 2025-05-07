#pragma once

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

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

  void send_turn_on();
  void send_turn_off();
  void send_set_temperature(float temperature);
  void send_set_mode(climate::ClimateMode mode);
  void send_set_fan_mode(climate::ClimateFanMode fan_mode);
  void send_heartbeat();

  void parse_uart_response();

  uint32_t last_heartbeat_{0};
  climate::ClimatePreset preset_{climate::CLIMATE_PRESET_NONE};

};

}  // namespace rotenso
}  // namespace esphome
