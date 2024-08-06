#pragma once

#include "esphome/components/climate/climate.h"
#include "esphome/components/climate/climate_mode.h"
#include "esppac.h"

namespace esphome {
namespace panasonic_ac {
namespace CNT {

static const uint8_t CTRL_HEADER = 0xF0;  // The header for control frames
static const uint8_t POLL_HEADER = 0x70;  // The header for the poll command

static const int POLL_INTERVAL = 5000;  // The interval at which to poll the AC
static const int CMD_INTERVAL = 250;  // The interval at which to send commands

enum class ACState {
  Initializing,  // Before first query response is receive
  Ready,         // All done, ready to receive regular packets
};

class PanasonicACCNT : public PanasonicAC {
 public:
  void control(const climate::ClimateCall &call) override;

  void on_horizontal_swing_change(const std::string &swing) override;
  void on_vertical_swing_change(const std::string &swing) override;
  void on_nanoex_change(bool nanoex) override;
  void on_eco_change(bool eco) override;
  void on_econavi_change(bool eco) override;
  void on_mild_dry_change(bool mild_dry) override;

  void setup() override;
  void loop() override;

  void dump_config() override;
 protected:
  ACState state_ = ACState::Initializing;  // Stores the internal state of the AC, used during initialization

  // uint8_t data[10];
  std::vector<uint8_t> data = std::vector<uint8_t>(10);  // Stores the data received from the AC
  std::vector<uint8_t> cmd;  // Used to build next command
  
  optional<climate::ClimateMode> lastMode;

  void handle_poll();
  void handle_cmd();

  void set_data(bool set);

  void send_command(std::vector<uint8_t> command, CommandType type, uint8_t header);
  void send_packet(const std::vector<uint8_t> &command, CommandType type);

  bool verify_packet();
  void handle_packet();

  climate::ClimateMode determine_mode(uint8_t mode);
  std::string determine_fan_speed(uint8_t speed);

  std::string determine_vertical_swing(uint8_t swing);
  std::string determine_horizontal_swing(uint8_t swing);

  std::string determine_preset(uint8_t preset);
  bool determine_preset_nanoex(uint8_t preset);
  bool determine_eco(uint8_t value);
  bool determine_econavi(uint8_t value);
  bool determine_mild_dry(uint8_t value);
  uint16_t determine_power_consumption(uint8_t byte_28, uint8_t multiplier, uint8_t offset);
};

}  // namespace CNT
}  // namespace panasonic_ac
}  // namespace esphome
