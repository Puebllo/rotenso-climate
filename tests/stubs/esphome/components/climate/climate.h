#pragma once

#include <cmath>
#include <initializer_list>
#include <optional>

namespace esphome {
namespace climate {

enum ClimateMode {
  CLIMATE_MODE_OFF, CLIMATE_MODE_HEAT, CLIMATE_MODE_COOL, CLIMATE_MODE_FAN_ONLY,
  CLIMATE_MODE_DRY, CLIMATE_MODE_AUTO,
};
enum ClimateFanMode { CLIMATE_FAN_LOW, CLIMATE_FAN_MEDIUM, CLIMATE_FAN_HIGH, CLIMATE_FAN_AUTO };
enum ClimatePreset { CLIMATE_PRESET_NONE, CLIMATE_PRESET_ECO, CLIMATE_PRESET_BOOST, CLIMATE_PRESET_COMFORT };

class ClimateCall {
 public:
  std::optional<ClimateMode> mode;
  std::optional<float> target_temperature;
  std::optional<ClimateFanMode> fan_mode;
  std::optional<ClimatePreset> preset;

  std::optional<ClimateMode> get_mode() const { return mode; }
  std::optional<float> get_target_temperature() const { return target_temperature; }
  std::optional<ClimateFanMode> get_fan_mode() const { return fan_mode; }
  std::optional<ClimatePreset> get_preset() const { return preset; }
};

class ClimateTraits {
 public:
  void set_supported_modes(std::initializer_list<ClimateMode>) {}
  void set_supported_fan_modes(std::initializer_list<ClimateFanMode>) {}
  void set_supported_presets(std::initializer_list<ClimatePreset>) {}
  void set_visual_min_temperature(float) {}
  void set_visual_max_temperature(float) {}
  void set_visual_temperature_step(float) {}
};

class Climate {
 public:
  virtual ~Climate() = default;
  ClimateMode mode{CLIMATE_MODE_OFF};
  float target_temperature{NAN};
  std::optional<ClimateFanMode> fan_mode;
  std::optional<ClimatePreset> preset;
  unsigned published_states{0};
  void publish_state() { published_states++; }

 protected:
  virtual ClimateTraits traits() = 0;
  virtual void control(const ClimateCall &) = 0;
};

inline const char *climate_fan_mode_to_string(ClimateFanMode) { return "fan"; }
}  // namespace climate
}  // namespace esphome
