#pragma once

#include "esphome/components/climate/climate.h"
#include "esphome/components/select/select.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

namespace esphome {

namespace panasonic_ac {

static const char *const VERSION = "2.4.0";

static const uint8_t BUFFER_SIZE = 128;  // The maximum size of a single packet (both receive and transmit)
static const uint8_t READ_TIMEOUT = 20;  // The maximum time to wait before considering a packet complete

static const uint8_t MIN_TEMPERATURE = 16;     // Minimum temperature as reported by Panasonic app
static const uint8_t MAX_TEMPERATURE = 30;     // Maximum temperature as supported by Panasonic app
static const float TEMPERATURE_STEP = 0.5;     // Steps the temperature can be set in
static const float TEMPERATURE_TOLERANCE = 2;  // The tolerance to allow when checking the climate state
static const uint8_t TEMPERATURE_THRESHOLD =
    100;  // Maximum temperature the AC can report before considering the temperature as invalid

enum class CommandType { Normal, Response, Resend };

enum class ACType {
  DNSKP11,  // New module (via CN-WLAN)
  CZTACG1   // Old module (via CN-CNT)
};

class PanasonicAC : public Component, public uart::UARTDevice, public climate::Climate {
 public:
  void set_outside_temperature_sensor(sensor::Sensor *outside_temperature_sensor);
  void set_vertical_swing_select(select::Select *vertical_swing_select);
  void set_horizontal_swing_select(select::Select *horizontal_swing_select);
  void set_nanoex_switch(switch_::Switch *nanoex_switch);
  void set_eco_switch(switch_::Switch *eco_switch);
  void set_econavi_switch(switch_::Switch *econavi_switch);
  void set_mild_dry_switch(switch_::Switch *mild_dry_switch);
  void set_current_power_consumption_sensor(sensor::Sensor *current_power_consumption_sensor);

  void set_current_temperature_sensor(sensor::Sensor *current_temperature_sensor);

  void setup() override;
  void loop() override;

 protected:
  sensor::Sensor *outside_temperature_sensor_ = nullptr;        // Sensor to store outside temperature from queries
  select::Select *vertical_swing_select_ = nullptr;             // Select to store manual position of vertical swing
  select::Select *horizontal_swing_select_ = nullptr;           // Select to store manual position of horizontal swing
  switch_::Switch *nanoex_switch_ = nullptr;                    // Switch to toggle nanoeX on/off
  switch_::Switch *eco_switch_ = nullptr;                       // Switch to toggle eco mode on/off
  switch_::Switch *econavi_switch_ = nullptr;                   // Switch to toggle econavi mode on/off
  switch_::Switch *mild_dry_switch_ = nullptr;                  // Switch to toggle mild dry mode on/off
  sensor::Sensor *current_temperature_sensor_ = nullptr;        // Sensor to use for current temperature where AC does not report
  sensor::Sensor *current_power_consumption_sensor_ = nullptr;  // Sensor to store current power consumption from queries

  std::string vertical_swing_state_;
  std::string horizontal_swing_state_;

  bool nanoex_state_ = false;    // Stores the state of nanoex to prevent duplicate packets
  bool eco_state_ = false;       // Stores the state of eco to prevent duplicate packets
  bool econavi_state_ = false;       // Stores the state of econavi to prevent duplicate packets
  bool mild_dry_state_ = false;  // Stores the state of mild dry to prevent duplicate packets

  bool waiting_for_response_ = false;  // Set to true if we are waiting for a response

  // uint8_t receive_buffer_index = 0;     // Current position of the receive buffer
  // uint8_t receive_buffer[BUFFER_SIZE];  // Stores the packet currently being received

  std::vector<uint8_t> rx_buffer_;

  uint32_t init_time_;             // Stores the current time
  uint32_t last_read_;             // Stores the time at which the last read was done
  uint32_t last_packet_sent_;      // Stores the time at which the last packet was sent
  uint32_t last_packet_received_;  // Stores the time at which the last packet was received

  climate::ClimateTraits traits() override;

  void read_data();

  void update_outside_temperature(int8_t temperature);
  void update_current_temperature(int8_t temperature);
  void update_target_temperature(uint8_t raw_value);
  void update_swing_horizontal(const std::string &swing);
  void update_swing_vertical(const std::string &swing);
  void update_nanoex(bool nanoex);
  void update_eco(bool eco);
  void update_econavi(bool econavi);
  void update_mild_dry(bool mild_dry);
  void update_current_power_consumption(int16_t power);

  virtual void on_horizontal_swing_change(const std::string &swing) = 0;
  virtual void on_vertical_swing_change(const std::string &swing) = 0;
  virtual void on_nanoex_change(bool nanoex) = 0;
  virtual void on_eco_change(bool eco) = 0;
  virtual void on_econavi_change(bool econavi) = 0;
  virtual void on_mild_dry_change(bool mild_dry) = 0;

  climate::ClimateAction determine_action();

  void log_packet(std::vector<uint8_t> data, bool outgoing = false);
};

}  // namespace panasonic_ac
}  // namespace esphome
