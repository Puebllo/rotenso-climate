#include "rotenso.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

uint32_t fake_millis = 0;

using namespace esphome;
using namespace esphome::climate;
using esphome::rotenso::RotensoClimate;

class TestClimate : public RotensoClimate {
 public:
  void call(const ClimateCall &call) { this->control(call); }
};

static std::vector<uint8_t> status(uint8_t command, ClimateMode mode, ClimateFanMode fan,
                                   float temperature, ClimatePreset preset = CLIMATE_PRESET_NONE) {
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
  assert(command[9] == 0x55 && command[10] == 0x05 && command[11] == 0x08);
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
  assert(command[8] == 0x01 && command[9] == 0x56 && command[10] == 0x05 && command[11] == 0x0A);
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

int main() {
  test_initial_state_fragmentation_and_preserved_fan();
  test_rapid_changes_and_old_heartbeat();
  test_changes_are_coalesced_while_initial_state_is_pending();
  test_commands_and_scheduled_heartbeats_do_not_overlap();
  test_timeout_retry_and_resync();
  test_corruption_and_timeout_resynchronization();
  test_retry_response_is_followed_by_fresh_poll();
  test_millis_wraparound();
  std::cout << "UART flow tests passed\n";
}
