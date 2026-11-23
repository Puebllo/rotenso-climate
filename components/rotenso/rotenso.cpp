#include "rotenso.h"
#include "frame_parser.h"
#include "rotenso_frame_builder.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cmath>

namespace esphome
{
  namespace rotenso
  {

    static const char *const TAG = "rotenso.climate";
    static constexpr uint32_t HEARTBEAT_INTERVAL_MS = 3000;
    static constexpr uint32_t RESPONSE_TIMEOUT_MS = 1000;
    static constexpr uint32_t RX_FRAME_TIMEOUT_MS = 500;
    static constexpr uint32_t BUS_QUIET_MS = 50;
    static constexpr uint8_t MAX_CONTROL_RETRIES = 1;

    void RotensoClimate::setup()
    {
      ESP_LOGI(TAG, "Rotenso climate setup complete");
      this->last_bus_activity_ = millis() - BUS_QUIET_MS;
    }

    void RotensoClimate::loop()
    {
      this->parse_uart_response();
      uint32_t now = millis();

      if (!this->rx_buffer_.empty() && now - this->last_rx_byte_ >= RX_FRAME_TIMEOUT_MS)
      {
        ESP_LOGW(TAG, "Incomplete UART frame; looking for the next frame");
        this->rx_buffer_.erase(this->rx_buffer_.begin());
        this->process_rx_buffer();
        // A second incomplete candidate must also get a timeout, without blocking forever.
        this->last_rx_byte_ = now;
      }

      // Do not transmit while the AC is sending a frame or immediately after bus activity.
      if (!this->rx_buffer_.empty() || now - this->last_bus_activity_ < BUS_QUIET_MS)
        return;

      if (this->request_ != Request::NONE && now - this->request_started_ >= RESPONSE_TIMEOUT_MS)
      {
        if (this->request_ == Request::CONTROL && this->control_retries_ < MAX_CONTROL_RETRIES)
        {
          ESP_LOGW(TAG, "No control response; retrying once");
          this->write_array(this->control_frame_.data(), this->control_frame_.size());
          this->control_retries_++;
          this->request_started_ = this->last_bus_activity_ = millis();
        }
        else
        {
          if (this->request_ == Request::CONTROL)
          {
            ESP_LOGW(TAG, "No control response after retry; requesting a fresh state");
            this->need_status_refresh_ = true;
            this->force_heartbeat_ = true;
          }
          else
          {
            ESP_LOGW(TAG, "No heartbeat response");
          }
          this->request_ = Request::NONE;
        }
      }

      if (this->request_ != Request::NONE)
        return;

      if (this->pending_control_.any() && this->have_state_ && !this->need_status_refresh_)
      {
        this->send_control();
      }
      else if (this->force_heartbeat_ || now - this->last_heartbeat_ >= HEARTBEAT_INTERVAL_MS)
      {
        this->send_heartbeat();
      }
    }

    void RotensoClimate::update(){}

    climate::ClimateTraits RotensoClimate::traits()
    {
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

      traits.set_supported_swing_modes({
          climate::CLIMATE_SWING_OFF,
          climate::CLIMATE_SWING_VERTICAL,
          climate::CLIMATE_SWING_HORIZONTAL,
          climate::CLIMATE_SWING_BOTH,
      });

      traits.set_visual_min_temperature(16);
      traits.set_visual_max_temperature(31);
      traits.set_visual_temperature_step(0.5);
      return traits;
    }

    void RotensoClimate::control(const climate::ClimateCall &call)
    {
      if (call.get_mode().has_value())
      {
        this->pending_control_.has_mode = true;
        this->pending_control_.mode = *call.get_mode();
      }

      if (call.get_target_temperature().has_value())
      {
        this->pending_control_.has_temperature = true;
        this->pending_control_.temperature = *call.get_target_temperature();
      }

      if (call.get_fan_mode().has_value())
      {
        this->pending_control_.has_fan_mode = true;
        this->pending_control_.fan_mode = *call.get_fan_mode();
      }

      if (call.get_preset().has_value())
      {
        this->pending_control_.has_preset = true;
        this->pending_control_.preset = *call.get_preset();
      }

      if (call.get_swing_mode().has_value())
      {
        auto mode = *call.get_swing_mode();
        auto vertical = this->get_desired_swing_state(true);
        auto horizontal = this->get_desired_swing_state(false);
        bool vertical_on = mode == climate::CLIMATE_SWING_VERTICAL || mode == climate::CLIMATE_SWING_BOTH;
        bool horizontal_on = mode == climate::CLIMATE_SWING_HORIZONTAL || mode == climate::CLIMATE_SWING_BOTH;
        // Preserve a selected range when it is already moving; otherwise use 1-5.
        if (vertical_on != vertical.moving)
          vertical.value = vertical_on ? 0x08 : vertical.value & 0x07;
        if (horizontal_on != horizontal.moving)
          horizontal.value = horizontal_on ? 0x08 : horizontal.value & 0x07;
        vertical.moving = vertical_on;
        horizontal.moving = horizontal_on;
        this->set_swing_state(true, vertical);
        this->set_swing_state(false, horizontal);
      }

      if (this->pending_control_.any())
        ESP_LOGD(TAG, "Queued climate change%s", this->have_state_ ? "" : "; waiting for initial AC state");
    }

    void RotensoClimate::set_swing_state(bool vertical, const SwingState &state)
    {
      if (vertical)
      {
        this->pending_control_.has_vertical_swing = true;
        this->pending_control_.vertical_swing = state;
      }
      else
      {
        this->pending_control_.has_horizontal_swing = true;
        this->pending_control_.horizontal_swing = state;
      }
      ESP_LOGD(TAG, "Queued %s louver change", vertical ? "vertical" : "horizontal");
    }

    SwingState RotensoClimate::get_desired_swing_state(bool vertical) const
    {
      if (vertical ? this->pending_control_.has_vertical_swing : this->pending_control_.has_horizontal_swing)
        return vertical ? this->pending_control_.vertical_swing : this->pending_control_.horizontal_swing;
      if (this->request_ == Request::CONTROL &&
          (vertical ? this->in_flight_control_.has_vertical_swing : this->in_flight_control_.has_horizontal_swing))
        return vertical ? this->in_flight_control_.vertical_swing : this->in_flight_control_.horizontal_swing;
      return this->get_swing_state(vertical);
    }

    void RotensoClimate::send_control()
    {
      this->in_flight_control_ = this->pending_control_;
      this->pending_control_ = PendingControl{};

      const auto &change = this->in_flight_control_;
      RotensoFrameBuilder builder;
      builder.from_climate_state(
          change.has_mode ? change.mode : this->mode,
          change.has_temperature ? change.temperature : this->target_temperature,
          change.has_fan_mode ? change.fan_mode : this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO),
          change.has_preset ? change.preset : this->preset.value_or(climate::CLIMATE_PRESET_NONE),
          change.has_vertical_swing ? change.vertical_swing : this->vertical_swing_,
          change.has_horizontal_swing ? change.horizontal_swing : this->horizontal_swing_);
      this->control_frame_ = builder.build_frame();
      ESP_LOGD(TAG, "Sending climate control command");
      this->write_array(this->control_frame_.data(), this->control_frame_.size());
      this->request_ = Request::CONTROL;
      this->control_retries_ = 0;
      this->request_started_ = this->last_bus_activity_ = millis();
    }

    void RotensoClimate::send_heartbeat()
    {
      ESP_LOGD(TAG, "Sending UART heartbeat");
      static const uint8_t heartbeat_packet[] = {0xBB, 0x00, 0x01, 0x04, 0x02, 0x01, 0x00, 0xBD};
      this->write_array(heartbeat_packet, sizeof(heartbeat_packet));
      this->request_ = Request::HEARTBEAT;
      this->force_heartbeat_ = false;
      this->request_started_ = this->last_bus_activity_ = this->last_heartbeat_ = millis();
    }

    void RotensoClimate::parse_uart_response()
    {
      uint8_t byte;
      while (this->available() && this->read_byte(&byte))
      {
        this->last_rx_byte_ = this->last_bus_activity_ = millis();
        this->rx_buffer_.push_back(byte);
        this->process_rx_buffer();
      }
    }

    void RotensoClimate::process_rx_buffer()
    {
      while (!this->rx_buffer_.empty())
      {
        auto start = std::find(this->rx_buffer_.begin(), this->rx_buffer_.end(), 0xBB);
        this->rx_buffer_.erase(this->rx_buffer_.begin(), start);
        if (this->rx_buffer_.size() < 3)
          return;

        if (this->rx_buffer_[1] != 0x01 || this->rx_buffer_[2] != 0x00)
        {
          this->rx_buffer_.erase(this->rx_buffer_.begin());
          continue;
        }
        if (this->rx_buffer_.size() < 5)
          return;

        const size_t frame_length = static_cast<size_t>(this->rx_buffer_[4]) + 6;
        if (this->rx_buffer_.size() < frame_length)
          return;

        uint8_t checksum = 0;
        for (size_t i = 0; i < frame_length; i++)
          checksum ^= this->rx_buffer_[i];
        if (checksum != 0)
        {
          ESP_LOGW(TAG, "Invalid UART checksum; looking for the next frame");
          this->rx_buffer_.erase(this->rx_buffer_.begin());
          continue;
        }

        std::vector<uint8_t> frame(this->rx_buffer_.begin(), this->rx_buffer_.begin() + frame_length);
        this->rx_buffer_.erase(this->rx_buffer_.begin(), this->rx_buffer_.begin() + frame_length);
        this->handle_frame(frame);
      }
    }

    void RotensoClimate::handle_frame(const std::vector<uint8_t> &frame)
    {
      ESP_LOGD(TAG, "Received UART frame: command 0x%02X, %u bytes", frame[3],
               static_cast<unsigned>(frame.size()));
      if (frame[3] != 0x03 && frame[3] != 0x04)
        return;

      auto parsed = parse_status(frame);
      if (!parsed.valid)
        return;

      // A delayed poll can contain the state from before an outstanding SET.
      if (this->request_ == Request::CONTROL && frame[3] == 0x04)
      {
        ESP_LOGD(TAG, "Ignoring heartbeat while awaiting a control response");
        return;
      }

      const bool control_response = this->request_ == Request::CONTROL && frame[3] == 0x03;
      const bool refresh_after_retry = control_response && this->control_retries_ > 0;
      if (control_response)
      {
        const auto &change = this->in_flight_control_;
        if ((change.has_mode && change.mode != parsed.mode) ||
            (change.has_temperature && std::fabs(change.temperature - parsed.temperature) > 0.01f) ||
            (change.has_fan_mode && change.fan_mode != parsed.fan_mode) ||
            (change.has_preset && change.preset != parsed.preset) ||
            (change.has_vertical_swing && !swing_states_match(true, change.vertical_swing, parsed.vertical_swing)) ||
            (change.has_horizontal_swing && !swing_states_match(false, change.horizontal_swing, parsed.horizontal_swing)))
        {
          ESP_LOGW(TAG, "AC control response differs from the requested state");
          if (change.has_mode && change.mode != parsed.mode)
            ESP_LOGW(TAG, "  Mode: requested %d, reported %d", static_cast<int>(change.mode), static_cast<int>(parsed.mode));
          if (change.has_temperature && std::fabs(change.temperature - parsed.temperature) > 0.01f)
            ESP_LOGW(TAG, "  Temperature: requested %.1f, reported %.1f", change.temperature, parsed.temperature);
          if (change.has_fan_mode && change.fan_mode != parsed.fan_mode)
            ESP_LOGW(TAG, "  Fan: requested %d, reported %d", static_cast<int>(change.fan_mode), static_cast<int>(parsed.fan_mode));
          if (change.has_preset && change.preset != parsed.preset)
            ESP_LOGW(TAG, "  Preset: requested %d, reported %d", static_cast<int>(change.preset), static_cast<int>(parsed.preset));
          if (change.has_vertical_swing && !swing_states_match(true, change.vertical_swing, parsed.vertical_swing))
            ESP_LOGW(TAG, "  Vertical louver: requested %s, reported %s", swing_option_from_state(true, change.vertical_swing), swing_option_from_state(true, parsed.vertical_swing));
          if (change.has_horizontal_swing && !swing_states_match(false, change.horizontal_swing, parsed.horizontal_swing))
            ESP_LOGW(TAG, "  Horizontal louver: requested %s, reported %s", swing_option_from_state(false, change.horizontal_swing), swing_option_from_state(false, parsed.horizontal_swing));
        }
      }

      this->mode = parsed.mode;
      this->fan_mode = parsed.fan_mode;
      this->target_temperature = parsed.temperature;
      this->preset = parsed.preset;
      this->vertical_swing_ = parsed.vertical_swing;
      this->horizontal_swing_ = parsed.horizontal_swing;
      this->swing_mode = parsed.vertical_swing.moving
          ? (parsed.horizontal_swing.moving ? climate::CLIMATE_SWING_BOTH : climate::CLIMATE_SWING_VERTICAL)
          : (parsed.horizontal_swing.moving ? climate::CLIMATE_SWING_HORIZONTAL : climate::CLIMATE_SWING_OFF);
      if (this->vertical_swing_select_ != nullptr)
        this->vertical_swing_select_->publish_swing_state(this->vertical_swing_);
      if (this->horizontal_swing_select_ != nullptr)
        this->horizontal_swing_select_->publish_swing_state(this->horizontal_swing_);
      this->have_state_ = true;
      if (frame[3] == 0x04 || (control_response && !refresh_after_retry))
        this->need_status_refresh_ = false;

      // A retried SET can produce two indistinguishable replies. Poll before the
      // next SET so a late duplicate cannot be mistaken for its response.
      if (refresh_after_retry)
      {
        this->need_status_refresh_ = true;
        this->force_heartbeat_ = true;
      }

      if ((this->request_ == Request::HEARTBEAT && frame[3] == 0x04) ||
          control_response)
        this->request_ = Request::NONE;

      this->publish_state();
    }

  } // namespace rotenso
} // namespace esphome
