#pragma once

#include <vector>
#include "esphome/components/climate/climate.h"
#include "rotenso_swing.h"

namespace esphome {
namespace rotenso {

struct ParsedClimateState {
    bool valid = false;
    esphome::climate::ClimateMode mode;
    esphome::climate::ClimateFanMode fan_mode;
    float temperature;
    esphome::climate::ClimatePreset preset = esphome::climate::CLIMATE_PRESET_NONE;
    SwingState vertical_swing;
    SwingState horizontal_swing;
};
      

ParsedClimateState parse_status(const std::vector<uint8_t> &buffer);

}  // namespace rotenso
}  // namespace esphome
