#include "rotenso.h"
#include "frame_parser.h"
#include "rotenso_frame_builder.h"
#include "esphome/core/log.h"

namespace esphome {
namespace rotenso {

static const char *const TAG = "rotenso.climate";

void RotensoClimate::setup() {
  ESP_LOGI(TAG, "Rotenso climate setup complete");
  this->set_update_interval(3000);
}

void RotensoClimate::loop() {
    static int32_t response_start_time = -1;
  
    if (response_start_time == -1 && this->available()) {
      response_start_time = millis();
      ESP_LOGD(TAG, "UART data detected, waiting to collect full response...");
    }
  
    if (response_start_time != -1 && millis() - response_start_time >= 500) {
      parse_uart_response();
      response_start_time = -1;
    }
  }

void RotensoClimate::update() {
    send_heartbeat();
  }

climate::ClimateTraits RotensoClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_supported_modes({
    climate::CLIMATE_MODE_OFF,
    climate::CLIMATE_MODE_HEAT,
    climate::CLIMATE_MODE_COOL,
    climate::CLIMATE_MODE_FAN_ONLY,
  });

  traits.set_supported_fan_modes({
    climate::CLIMATE_FAN_LOW,
    climate::CLIMATE_FAN_MEDIUM,
    climate::CLIMATE_FAN_HIGH,
    climate::CLIMATE_FAN_AUTO,
  });

  traits.set_supported_presets({
    climate::CLIMATE_PRESET_NONE,
    climate::CLIMATE_PRESET_ECO,
    climate::CLIMATE_PRESET_BOOST,
  });
  traits.set_supports_current_temperature(false);

  traits.set_visual_min_temperature(16);
  traits.set_visual_max_temperature(30);
  traits.set_visual_temperature_step(0.5);
  return traits;
}

void RotensoClimate::control(const climate::ClimateCall &call) {
    if (call.get_mode().has_value()) {
        this->mode = *call.get_mode();
      }
    
      if (call.get_target_temperature().has_value()) {
        this->target_temperature = *call.get_target_temperature();
      }
    
      if (call.get_preset().has_value()) {
        this->preset = *call.get_preset();
      }
    
      RotensoFrameBuilder builder;
      builder.from_climate_state(this);  // 'this' is your climate instance
      auto frame = builder.build_frame();
      this->write_array(frame.data(), frame.size());

//   if (call.get_mode().has_value()) {
//     auto mode = *call.get_mode();
//     if (mode == climate::CLIMATE_MODE_OFF) {
//       send_turn_off();
//     } else {
//       send_turn_on();
//       send_set_mode(mode);
//     }
//     this->mode = mode;
//   }

//   if (call.get_target_temperature().has_value()) {
//     float temp = *call.get_target_temperature();
//     send_set_temperature(temp);
//     this->target_temperature = temp;
//   }

//   if (call.get_fan_mode().has_value()) {
//     send_set_fan_mode(*call.get_fan_mode());
//     this->fan_mode = *call.get_fan_mode();
//   }

//   if (call.get_preset().has_value()) {
//     this->preset_ = *call.get_preset();
//   }

//   this->preset = this->preset_;

//   publish_state();
}

void RotensoClimate::send_turn_on() {
  ESP_LOGD(TAG, "Sending UART command: Turn ON");
  static const uint8_t on_packet[] = {0xBB, 0x00, 0x01, 0x0A, 0x03, 0x05, 0x00, 0x00, 0xB6};
  this->write_array(on_packet, sizeof(on_packet));
  this->flush();
  delay(90);
  static const uint8_t display_packet[] = {0xBB, 0x00, 0x01, 0x09, 0x02, 0x05, 0x00, 0xB4};
  this->write_array(display_packet, sizeof(display_packet));
  this->flush();
}

void RotensoClimate::send_turn_off() {
  ESP_LOGD(TAG, "Sending UART command: Turn OFF");
  static const uint8_t off_packet[] = {0xBB, 0x00, 0x01, 0x0A, 0x03, 0x05, 0x00, 0x00, 0xB6};
  this->write_array(off_packet, sizeof(off_packet));
  this->flush();
  delay(90);
  static const uint8_t display_packet[] = {0xBB, 0x00, 0x01, 0x09, 0x02, 0x05, 0x00, 0xB4};
  this->write_array(display_packet, sizeof(display_packet));
  this->flush();
}

void RotensoClimate::send_set_temperature(float temperature) {
  ESP_LOGD(TAG, "Sending UART command: Set temperature to %.1f", temperature);
  // TODO: Send actual UART command
}

void RotensoClimate::send_set_mode(climate::ClimateMode mode) {
  ESP_LOGD(TAG, "Sending UART command: Set mode to %d", mode);
  // TODO: Send actual UART command
}

void RotensoClimate::send_set_fan_mode(climate::ClimateFanMode fan_mode) {
  ESP_LOGD(TAG, "Sending UART command: Set fan mode to %d", fan_mode);
  // TODO: Send actual UART command
}

void RotensoClimate::send_heartbeat() {
  ESP_LOGD(TAG, "Sending UART heartbeat");
  static const uint8_t heartbeat_packet[] = {0xBB, 0x00, 0x01, 0x04, 0x02, 0x01, 0x00, 0xBD};
  this->write_array(heartbeat_packet, sizeof(heartbeat_packet));
  this->flush();
  delay(30);
}

void RotensoClimate::parse_uart_response() {
    size_t len = this->available();
    if (len == 0) {
      return;
    }
  
    std::vector<uint8_t> buffer;
    buffer.reserve(len);
  
    for (size_t i = 0; i < len; i++) {
      uint8_t byte;
      if (this->read_byte(&byte)) {
        buffer.push_back(byte);
      }
    }
  
    std::string log_line;
    char byte_str[6];
  
    for (size_t i = 0; i < buffer.size(); i++) {
      snprintf(byte_str, sizeof(byte_str), "0x%02X ", buffer[i]);
      log_line += byte_str;
    }
  
    ESP_LOGD(TAG, "UART response: %s", log_line.c_str());
  
    //if buffer size is 61 and command type is Get 0x4 then we assume it's heartbeat response 
    // (there is also buffer size 51 and command type 0x9 which is to figure out)

    if(buffer.size() == 61 && buffer[3] == 0x04){
        auto parsed = parse_heartbeat(buffer);
        if (parsed.valid) {
            this->mode = parsed.mode;
            this->fan_mode = parsed.fan_mode;
            this->target_temperature = parsed.temperature;
            //fixme: this->current_temperature = parsed.temperature;
             
            this->preset = parsed.preset;
            ESP_LOGI(TAG, "Updated climate state from heartbeat");

            this->publish_state();
          }
    }
  }
  

}  // namespace rotenso
}  // namespace esphome
