#pragma once

#include "esphome/components/climate/climate.h"
#include "esphome/components/select/select.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

#include "panasonic_ac_traits_builder.h"

namespace esphome {
namespace panasonic_ac {

static const uint8_t BUFFER_SIZE = 128;  // The maximum size of a single packet (both receive and transmit)
static const uint8_t READ_TIMEOUT = 20;  // The maximum time to wait before considering a packet complete

enum class CommandType { Normal, Response, Resend };

class PanasonicAC : public Component, public uart::UARTDevice, public climate::Climate {
 public:
  PanasonicACTraitsBuilder &get_traits_builder() {
    return panasonic_ac_traits_builder_;
  }

  void set_vertical_swing_select(select::Select *vertical_swing_select);
  void set_horizontal_swing_select(select::Select *horizontal_swing_select);
  void set_nanoex_switch(switch_::Switch *nanoex_switch);
  void set_outside_temperature_sensor(sensor::Sensor *outside_temperature_sensor);
  void set_current_power_consumption_sensor(sensor::Sensor *current_power_consumption_sensor);

  void setup() override;
  void loop() override;
  void dump_config() override;
 protected:
  select::Select *vertical_swing_select_ = nullptr;             // Select to store manual position of vertical swing
  select::Select *horizontal_swing_select_ = nullptr;           // Select to store manual position of horizontal swing
  switch_::Switch *nanoex_switch_ = nullptr;                    // Switch to toggle nanoeX on/off
  
  sensor::Sensor *current_power_consumption_sensor_ = nullptr;  // Sensor to store current power consumption from queries
  sensor::Sensor *outside_temperature_sensor_ = nullptr;        // Sensor to store outside temperature from queries

  std::string vertical_swing_state_;
  std::string horizontal_swing_state_;
  bool nanoex_state_ = false;    // Stores the state of nanoex to prevent duplicate packets
  

  bool waiting_for_response_ = false;  // Set to true if we are waiting for a response

  // uint8_t receive_buffer_index = 0;     // Current position of the receive buffer
  // uint8_t receive_buffer[BUFFER_SIZE];  // Stores the packet currently being received

  std::vector<uint8_t> rx_buffer_;

  uint32_t init_time_;             // Stores the current time
  uint32_t last_read_;             // Stores the time at which the last read was done
  uint32_t last_packet_sent_;      // Stores the time at which the last packet was sent
  uint32_t last_packet_received_;  // Stores the time at which the last packet was received

  PanasonicACTraitsBuilder panasonic_ac_traits_builder_ = PanasonicACTraitsBuilder();
  climate::ClimateTraits panasonic_ac_traits_;

  climate::ClimateTraits traits() override {
    return panasonic_ac_traits_;
  }

  void read_data();

  void update_outside_temperature(int8_t temperature);
  void update_current_temperature(int8_t temperature);
  void update_target_temperature(uint8_t raw_value);
  void update_swing_horizontal(const std::string &swing);
  void update_swing_vertical(const std::string &swing);
  void update_nanoex(bool nanoex);
  void update_current_power_consumption(int16_t power);

  virtual void on_horizontal_swing_change(const std::string &swing) = 0;
  virtual void on_vertical_swing_change(const std::string &swing) = 0;
  virtual void on_nanoex_change(bool nanoex) = 0;

  climate::ClimateAction determine_action();

  void log_packet(std::vector<uint8_t> data, bool outgoing = false);
};

}  // namespace panasonic_ac
}  // namespace esphome
