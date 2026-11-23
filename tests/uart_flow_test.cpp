#include "rotenso.h"
#include "frame_parser.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

uint32_t fake_millis = 0;

using namespace esphome;
using namespace esphome::climate;
using esphome::rotenso::RotensoClimate;
using esphome::rotenso::RotensoFrameBuilder;
using esphome::rotenso::RotensoSwingSelect;
using esphome::rotenso::SwingState;

class TestClimate : public RotensoClimate {
 public:
  void call(const ClimateCall &call) { this->control(call); }
};

static std::vector<uint8_t> status(uint8_t command, ClimateMode mode, ClimateFanMode fan,
                                   float temperature, ClimatePreset preset = CLIMATE_PRESET_NONE,
                                   SwingState vertical = {}, SwingState horizontal = {}) {
  std::vector<uint8_t> frame(61, 0);
  frame[0] = 0xBB;
  frame[1] = 0x01;
  frame[2] = 0x00;
  frame[3] = command;
  frame[4] = 55;
  frame[5] = 0x04;
  uint8_t mode_code = mode == CLIMATE_MODE_HEAT ? 4 : mode == CLIMATE_MODE_FAN_ONLY ? 2 : 1;
  uint8_t state = mode == CLIMATE_MODE_OFF ? 0x20 : preset == CLIMATE_PRESET_ECO ? 0x70 :
                                                   preset == CLIMATE_PRESET_BOOST ? 0xB0 : 0x30;
  frame[7] = state | mode_code;
  uint8_t fan_code = fan == CLIMATE_FAN_LOW ? 0x90 : fan == CLIMATE_FAN_MEDIUM ? 0xA0 :
                     fan == CLIMATE_FAN_HIGH ? 0xB0 : 0x80;
  int whole = static_cast<int>(temperature);
  frame[8] = fan_code | static_cast<uint8_t>(whole - 16);
  frame[9] = temperature - whole > 0.25f ? 0x02 : 0x00;
  frame[10] = (vertical.moving ? 0x40 : 0) | (horizontal.moving ? 0x20 : 0);
  frame[51] = vertical.value;
  frame[52] = horizontal.value;
  for (size_t i = 0; i + 1 < frame.size(); i++) frame.back() ^= frame[i];
  return frame;
}

static std::vector<uint8_t> short_response() {
  std::vector<uint8_t> frame(51, 0);
  frame[0] = 0xBB;
  frame[1] = 0x01;
  frame[3] = 0x09;
  frame[4] = 45;
  for (size_t i = 0; i + 1 < frame.size(); i++) frame.back() ^= frame[i];
  return frame;
}

static void receive(TestClimate &climate, const std::vector<uint8_t> &bytes, uint32_t time) {
  fake_millis = time;
  climate.received.insert(climate.received.end(), bytes.begin(), bytes.end());
  climate.loop();
}

static void tick(TestClimate &climate, uint32_t time) {
  fake_millis = time;
  climate.loop();
}

static void assert_checksum(const std::vector<uint8_t> &frame) {
  uint8_t checksum = 0;
  for (uint8_t byte : frame) checksum ^= byte;
  assert(checksum == 0);
}

static void test_initial_state_fragmentation_and_preserved_fan() {
  fake_millis = 100;
  TestClimate climate;
  climate.setup();
  climate.loop();
  assert(climate.transmitted.size() == 1 && climate.transmitted[0][3] == 0x04);

  ClimateCall change;
  change.target_temperature = 26.0f;
  climate.call(change);
  auto state = status(0x04, CLIMATE_MODE_HEAT, CLIMATE_FAN_HIGH, 24.5f);
  receive(climate, std::vector<uint8_t>(state.begin(), state.begin() + 20), 110);
  assert(climate.transmitted.size() == 1 && climate.published_states == 0);

  std::vector<uint8_t> remainder(state.begin() + 20, state.end());
  auto diagnostic = short_response();
  remainder.insert(remainder.end(), diagnostic.begin(), diagnostic.end());
  receive(climate, remainder, 120);
  assert(climate.published_states == 1 && climate.target_temperature == 24.5f);
  assert(climate.transmitted.size() == 1);
  tick(climate, 170);
  assert(climate.transmitted.size() == 2);
  const auto &command = climate.transmitted.back();
  assert(command.size() == 39 && command[3] == 0x03);
  assert(command[7] == 0x64 && command[8] == 0x01);
  assert(command[9] == 0x55 && command[10] == 0x05 && command[11] == 0x00);
  assert_checksum(command);

  receive(climate, status(0x03, CLIMATE_MODE_HEAT, CLIMATE_FAN_HIGH, 26.0f), 180);
  assert(climate.published_states == 2 && climate.target_temperature == 26.0f);
}

static void test_rapid_changes_and_old_heartbeat() {
  fake_millis = 100;
  TestClimate climate;
  climate.setup();
  climate.loop();
  receive(climate, status(0x04, CLIMATE_MODE_COOL, CLIMATE_FAN_LOW, 24), 110);

  ClimateCall first;
  first.mode = CLIMATE_MODE_HEAT;
  climate.call(first);
  tick(climate, 160);
  assert(climate.transmitted.size() == 2);

  ClimateCall second;
  second.target_temperature = 25.5f;
  second.fan_mode = CLIMATE_FAN_HIGH;
  climate.call(second);
  receive(climate, status(0x04, CLIMATE_MODE_COOL, CLIMATE_FAN_LOW, 24), 170);
  assert(climate.published_states == 1 && climate.transmitted.size() == 2);
  receive(climate, status(0x03, CLIMATE_MODE_HEAT, CLIMATE_FAN_LOW, 24), 180);
  assert(climate.published_states == 2 && climate.mode == CLIMATE_MODE_HEAT);
  tick(climate, 230);
  assert(climate.transmitted.size() == 3);
  const auto &command = climate.transmitted.back();
  assert(command[8] == 0x01 && command[9] == 0x56 && command[10] == 0x05 && command[11] == 0x02);
  assert_checksum(command);
  receive(climate, status(0x03, CLIMATE_MODE_HEAT, CLIMATE_FAN_HIGH, 25.5f), 240);
  assert(climate.published_states == 3 && climate.target_temperature == 25.5f);
}

static void test_changes_are_coalesced_while_initial_state_is_pending() {
  fake_millis = 100;
  TestClimate climate;
  climate.setup();
  climate.loop();
  ClimateCall first;
  first.mode = CLIMATE_MODE_HEAT;
  first.target_temperature = 25;
  first.fan_mode = CLIMATE_FAN_LOW;
  climate.call(first);
  ClimateCall second;
  second.target_temperature = 27;
  second.fan_mode = CLIMATE_FAN_HIGH;
  climate.call(second);
  receive(climate, status(0x04, CLIMATE_MODE_COOL, CLIMATE_FAN_MEDIUM, 24), 110);
  tick(climate, 160);
  assert(climate.transmitted.size() == 2);
  const auto &command = climate.transmitted.back();
  assert(command[8] == 0x01 && command[9] == 0x54 && command[10] == 0x05);
}

static void test_commands_and_scheduled_heartbeats_do_not_overlap() {
  fake_millis = 100;
  TestClimate climate;
  climate.setup();
  climate.loop();
  auto current = status(0x04, CLIMATE_MODE_HEAT, CLIMATE_FAN_HIGH, 24);
  receive(climate, current, 110);

  // A scheduled heartbeat is already in flight when HA sends a command.
  tick(climate, 3100);
  assert(climate.transmitted.size() == 2 && climate.transmitted.back()[3] == 0x04);
  ClimateCall first;
  first.target_temperature = 25;
  climate.call(first);
  tick(climate, 3150);
  assert(climate.transmitted.size() == 2);
  receive(climate, current, 3160);
  tick(climate, 3210);
  assert(climate.transmitted.size() == 3 && climate.transmitted.back()[3] == 0x03);
  receive(climate, status(0x03, CLIMATE_MODE_HEAT, CLIMATE_FAN_HIGH, 25), 3220);

  // A control command is in flight when the next heartbeat becomes due.
  ClimateCall second;
  second.target_temperature = 26;
  climate.call(second);
  tick(climate, 6050);
  assert(climate.transmitted.size() == 4 && climate.transmitted.back()[3] == 0x03);
  tick(climate, 6100);
  assert(climate.transmitted.size() == 4);
  receive(climate, status(0x03, CLIMATE_MODE_HEAT, CLIMATE_FAN_HIGH, 26), 6110);
  tick(climate, 6160);
  assert(climate.transmitted.size() == 5 && climate.transmitted.back()[3] == 0x04);
}

static void test_timeout_retry_and_resync() {
  fake_millis = 100;
  TestClimate climate;
  climate.setup();
  climate.loop();
  receive(climate, status(0x04, CLIMATE_MODE_HEAT, CLIMATE_FAN_MEDIUM, 24), 110);
  ClimateCall first;
  first.target_temperature = 26;
  climate.call(first);
  tick(climate, 160);
  ClimateCall second;
  second.mode = CLIMATE_MODE_COOL;
  climate.call(second);

  tick(climate, 1160);
  assert(climate.transmitted.size() == 3 && climate.transmitted[1] == climate.transmitted[2]);
  tick(climate, 2160);
  assert(climate.transmitted.size() == 4 && climate.transmitted.back()[3] == 0x04);
  tick(climate, 2200);
  assert(climate.transmitted.size() == 4);
  receive(climate, status(0x04, CLIMATE_MODE_HEAT, CLIMATE_FAN_MEDIUM, 24), 2210);
  tick(climate, 2260);
  assert(climate.transmitted.size() == 5);
  assert(climate.transmitted.back()[8] == 0x03 && climate.transmitted.back()[9] == 0x57);
  assert(climate.transmitted.back()[10] == 0x03);
}

static void test_corruption_and_timeout_resynchronization() {
  fake_millis = 100;
  TestClimate climate;
  climate.setup();
  climate.loop();
  auto bad = status(0x04, CLIMATE_MODE_COOL, CLIMATE_FAN_LOW, 20);
  bad.back() ^= 0x01;
  auto good = status(0x04, CLIMATE_MODE_HEAT, CLIMATE_FAN_HIGH, 23);
  bad.insert(bad.end(), good.begin(), good.end());
  receive(climate, bad, 110);
  assert(climate.published_states == 1 && climate.mode == CLIMATE_MODE_HEAT);

  std::vector<uint8_t> incomplete = {0xBB, 0x01, 0x00, 0x09, 200};
  auto next = status(0x04, CLIMATE_MODE_COOL, CLIMATE_FAN_LOW, 20);
  incomplete.insert(incomplete.end(), next.begin(), next.end());
  receive(climate, incomplete, 200);
  assert(climate.published_states == 1);
  tick(climate, 700);
  assert(climate.published_states == 2 && climate.mode == CLIMATE_MODE_COOL);
}

static void test_retry_response_is_followed_by_fresh_poll() {
  fake_millis = 100;
  TestClimate climate;
  climate.setup();
  climate.loop();
  receive(climate, status(0x04, CLIMATE_MODE_HEAT, CLIMATE_FAN_HIGH, 24), 110);
  ClimateCall first;
  first.target_temperature = 26;
  climate.call(first);
  tick(climate, 160);
  ClimateCall second;
  second.fan_mode = CLIMATE_FAN_LOW;
  climate.call(second);

  tick(climate, 1160);
  assert(climate.transmitted.size() == 3 && climate.transmitted[1] == climate.transmitted[2]);
  auto ack = status(0x03, CLIMATE_MODE_HEAT, CLIMATE_FAN_HIGH, 26);
  receive(climate, ack, 1170);
  tick(climate, 1220);
  assert(climate.transmitted.size() == 4 && climate.transmitted.back()[3] == 0x04);
  receive(climate, ack, 1230);  // A duplicate SET response is not a poll response.
  tick(climate, 1280);
  assert(climate.transmitted.size() == 4);
  receive(climate, status(0x04, CLIMATE_MODE_HEAT, CLIMATE_FAN_HIGH, 26), 1290);
  tick(climate, 1340);
  assert(climate.transmitted.size() == 5 && climate.transmitted.back()[3] == 0x03);
  assert(climate.transmitted.back()[9] == 0x55 && climate.transmitted.back()[10] == 0x02);
}

static void test_millis_wraparound() {
  fake_millis = UINT32_MAX - 20;
  TestClimate climate;
  climate.setup();
  climate.loop();
  receive(climate, status(0x04, CLIMATE_MODE_HEAT, CLIMATE_FAN_HIGH, 24), UINT32_MAX - 10);
  ClimateCall change;
  change.target_temperature = 25;
  climate.call(change);
  tick(climate, 39);
  assert(climate.transmitted.size() == 2 && climate.transmitted.back()[3] == 0x03);
}

static void test_captured_swing_commands() {
  // SetCommand rows 19-36 in the supplied workbook, including their checksums.
  const std::array<uint8_t, 39> baseline = {
      0xBB, 0x00, 0x01, 0x03, 0x21, 0x00, 0x00, 0x64, 0x03, 0x5B,
      0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x27};
  struct Capture {
    bool vertical;
    const char *option;
    uint8_t fan_byte;
    uint8_t temperature_swing_byte;
    uint8_t vertical_byte;
    uint8_t horizontal_byte;
    uint8_t checksum;
  };
  const Capture captures[] = {
      {true, "Stopped",    0x03, 0x00, 0x00, 0x80, 0x27},
      {true, "Swing 1-5",  0x3B, 0x00, 0x08, 0x80, 0x17},
      {true, "Swing 3-5",  0x3B, 0x00, 0x10, 0x80, 0x0F},
      {true, "Swing 1-3",  0x3B, 0x00, 0x18, 0x80, 0x07},
      {true, "Position 1", 0x03, 0x00, 0x05, 0x80, 0x22},
      {true, "Position 2", 0x03, 0x00, 0x04, 0x80, 0x23},
      {true, "Position 3", 0x03, 0x00, 0x03, 0x80, 0x24},
      {true, "Position 4", 0x03, 0x00, 0x02, 0x80, 0x25},
      {true, "Position 5", 0x03, 0x00, 0x01, 0x80, 0x26},
      {false, "Swing 1-5",  0x03, 0x08, 0x01, 0x88, 0x26},
      {false, "Swing 1-3",  0x03, 0x08, 0x01, 0x90, 0x3E},
      {false, "Swing 2-4",  0x03, 0x08, 0x01, 0x98, 0x36},
      {false, "Swing 3-5",  0x03, 0x08, 0x01, 0xA0, 0x0E},
      {false, "Position 1", 0x03, 0x00, 0x00, 0x81, 0x26},
      {false, "Position 2", 0x03, 0x00, 0x00, 0x82, 0x25},
      {false, "Position 3", 0x03, 0x00, 0x00, 0x83, 0x24},
      {false, "Position 4", 0x03, 0x00, 0x00, 0x84, 0x23},
      {false, "Position 5", 0x03, 0x00, 0x00, 0x85, 0x22},
  };
  for (const auto &capture : captures) {
    SwingState requested;
    assert(esphome::rotenso::swing_state_from_option(capture.vertical, capture.option, requested));
    SwingState vertical = capture.vertical ? requested : SwingState{capture.vertical_byte, false};
    SwingState horizontal = capture.vertical ? SwingState{} : requested;
    RotensoFrameBuilder builder;
    builder.from_climate_state(CLIMATE_MODE_COOL, 20, CLIMATE_FAN_MEDIUM, CLIMATE_PRESET_NONE,
                               vertical, horizontal);
    auto expected = baseline;
    expected[10] = capture.fan_byte;
    expected[11] = capture.temperature_swing_byte;
    expected[32] = capture.vertical_byte;
    expected[33] = capture.horizontal_byte;
    expected[38] = capture.checksum;
    assert(builder.build_frame() == expected);
    assert(std::string(esphome::rotenso::swing_option_from_state(capture.vertical, requested)) == capture.option);
  }

  // SetCommand row 5: both axes moving with a half-degree target.
  RotensoFrameBuilder builder;
  builder.from_climate_state(CLIMATE_MODE_COOL, 20.5f, CLIMATE_FAN_HIGH, CLIMATE_PRESET_NONE,
                             SwingState{0x08, true}, SwingState{0x08, true});
  auto expected = baseline;
  expected[10] = 0x3D;
  expected[11] = 0x0A;
  expected[32] = 0x08;
  expected[33] = 0x88;
  expected[38] = 0x13;
  assert(builder.build_frame() == expected);
}

static void test_captured_swing_response() {
  // Heartbeat sheet row 60: vertical swing between positions 1 and 3.
  const std::vector<uint8_t> captured = {
      0xBB, 0x01, 0x00, 0x03, 0x37, 0x04, 0x00, 0x31, 0xA4, 0x00,
      0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x69, 0x03, 0x88,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x69, 0xFF, 0x42, 0x00, 0x73, 0x29, 0x2A, 0x2C, 0x23, 0x1C,
      0x8A, 0x00, 0x00, 0x00, 0x00, 0xEF, 0x00, 0x00, 0x00, 0x44,
      0x40, 0x18, 0x00, 0x00, 0x00, 0x7A, 0x00, 0x00, 0x00, 0x00, 0x09};
  assert_checksum(captured);
  auto parsed = esphome::rotenso::parse_status(captured);
  assert(parsed.valid && parsed.vertical_swing.moving && !parsed.horizontal_swing.moving);
  assert(std::string(esphome::rotenso::swing_option_from_state(true, parsed.vertical_swing)) == "Swing 1-3");
  assert(std::string(esphome::rotenso::swing_option_from_state(false, parsed.horizontal_swing)) == "Stopped");

  // Response bytes from Heartbeat rows 58-74, in the workbook's position numbering.
  struct ResponseCase { bool vertical; uint8_t flags; uint8_t value; const char *option; };
  const ResponseCase cases[] = {
      {true, 0x40, 0x08, "Swing 1-5"}, {true, 0x40, 0x10, "Swing 3-5"},
      {true, 0x40, 0x18, "Swing 1-3"}, {true, 0x00, 0x05, "Position 1"},
      {true, 0x00, 0x04, "Position 2"}, {true, 0x00, 0x03, "Position 3"},
      {true, 0x00, 0x02, "Position 4"}, {true, 0x00, 0x01, "Position 5"},
      {false, 0x20, 0x08, "Swing 1-5"}, {false, 0x20, 0x10, "Swing 1-3"},
      {false, 0x20, 0x18, "Swing 2-4"}, {false, 0x20, 0x20, "Swing 3-5"},
      {false, 0x00, 0x01, "Position 1"}, {false, 0x00, 0x02, "Position 2"},
      {false, 0x00, 0x03, "Position 3"}, {false, 0x00, 0x04, "Position 4"},
      {false, 0x00, 0x05, "Position 5"},
  };
  for (const auto &entry : cases) {
    auto frame = captured;
    frame[10] = entry.flags;
    frame[51] = entry.vertical ? entry.value : 0;
    frame[52] = entry.vertical ? 0 : entry.value;
    frame.back() = 0;
    for (size_t i = 0; i + 1 < frame.size(); i++) frame.back() ^= frame[i];
    assert_checksum(frame);
    auto result = esphome::rotenso::parse_status(frame);
    const auto &swing = entry.vertical ? result.vertical_swing : result.horizontal_swing;
    assert(result.valid && std::string(esphome::rotenso::swing_option_from_state(entry.vertical, swing)) == entry.option);
  }
  assert(esphome::rotenso::swing_states_match(true, SwingState{0x10, true}, SwingState{0x15, true}));
  assert(!esphome::rotenso::swing_states_match(false, SwingState{0x10, true}, SwingState{0x18, true}));
}

static void test_swing_queue_and_preservation() {
  fake_millis = 100;
  TestClimate climate;
  RotensoSwingSelect vertical;
  RotensoSwingSelect horizontal;
  climate.set_vertical_swing_select(&vertical);
  climate.set_horizontal_swing_select(&horizontal);
  climate.setup();
  climate.loop();
  receive(climate, status(0x04, CLIMATE_MODE_HEAT, CLIMATE_FAN_HIGH, 24, CLIMATE_PRESET_NONE,
                          SwingState{0x18, true}, SwingState{0x04, false}), 110);
  assert(vertical.state == "Swing 1-3" && horizontal.state == "Position 4");
  assert(climate.swing_mode == CLIMATE_SWING_VERTICAL);

  horizontal.make_test_call("Swing 2-4");
  tick(climate, 160);
  auto command = climate.transmitted.back();
  assert(command[10] == 0x3D && command[11] == 0x08 && command[32] == 0x18 && command[33] == 0x98);

  // Queue a position and a temperature while the previous swing command is in flight.
  vertical.make_test_call("Position 1");
  ClimateCall temperature;
  temperature.target_temperature = 25.5f;
  climate.call(temperature);
  receive(climate, status(0x03, CLIMATE_MODE_HEAT, CLIMATE_FAN_HIGH, 24, CLIMATE_PRESET_NONE,
                          SwingState{0x18, true}, SwingState{0x18, true}), 170);
  assert(vertical.state == "Swing 1-3" && horizontal.state == "Swing 2-4");
  tick(climate, 220);
  command = climate.transmitted.back();
  assert(command[9] == 0x56 && command[10] == 0x05 && command[11] == 0x0A);
  assert(command[32] == 0x05 && command[33] == 0x98);
  receive(climate, status(0x03, CLIMATE_MODE_HEAT, CLIMATE_FAN_HIGH, 25.5f, CLIMATE_PRESET_NONE,
                          SwingState{0x05, false}, SwingState{0x18, true}), 230);
  assert(vertical.state == "Position 1" && horizontal.state == "Swing 2-4");
  assert(climate.swing_mode == CLIMATE_SWING_HORIZONTAL);

  // Climate's generic BOTH mode enables the stationary axis without losing the other axis's range.
  ClimateCall both;
  both.swing_mode = CLIMATE_SWING_BOTH;
  climate.call(both);
  tick(climate, 280);
  command = climate.transmitted.back();
  assert(command[10] == 0x3D && command[11] == 0x0A && command[32] == 0x08 && command[33] == 0x98);
  receive(climate, status(0x03, CLIMATE_MODE_HEAT, CLIMATE_FAN_HIGH, 25.5f, CLIMATE_PRESET_NONE,
                          SwingState{0x08, true}, SwingState{0x18, true}), 290);
  assert(climate.swing_mode == CLIMATE_SWING_BOTH);
}

static void test_unknown_swing_preserves_flags() {
  fake_millis = 100;
  TestClimate climate;
  RotensoSwingSelect vertical;
  RotensoSwingSelect horizontal;
  climate.set_vertical_swing_select(&vertical);
  climate.set_horizontal_swing_select(&horizontal);
  climate.setup();
  climate.loop();
  receive(climate, status(0x04, CLIMATE_MODE_COOL, CLIMATE_FAN_LOW, 20, CLIMATE_PRESET_NONE,
                          SwingState{0, true}, SwingState{0, true}), 110);
  assert(vertical.state == "Unknown" && horizontal.state == "Unknown");
  assert(climate.swing_mode == CLIMATE_SWING_BOTH);
  ClimateCall change;
  change.target_temperature = 21;
  climate.call(change);
  tick(climate, 160);
  auto command = climate.transmitted.back();
  assert(command[10] == 0x3A && command[11] == 0x08 && command[32] == 0x00 && command[33] == 0x80);
  horizontal.make_test_call("Unknown");
  tick(climate, 220);
  assert(climate.transmitted.size() == 2);
}

int main() {
  test_initial_state_fragmentation_and_preserved_fan();
  test_rapid_changes_and_old_heartbeat();
  test_changes_are_coalesced_while_initial_state_is_pending();
  test_commands_and_scheduled_heartbeats_do_not_overlap();
  test_timeout_retry_and_resync();
  test_corruption_and_timeout_resynchronization();
  test_retry_response_is_followed_by_fresh_poll();
  test_millis_wraparound();
  test_captured_swing_commands();
  test_captured_swing_response();
  test_swing_queue_and_preservation();
  test_unknown_swing_preserves_flags();
  std::cout << "UART flow tests passed\n";
}
