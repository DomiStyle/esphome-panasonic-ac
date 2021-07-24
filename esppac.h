#pragma once

#include "esppac.h"

using namespace esphome;

namespace ESPPAC
{
  static const char* VERSION = "2.0.0";
  static const char* TAG = "esppac";

  static const int BUFFER_SIZE = 128; // The maximum size of a single packet (both receive and transmit)
  static const int READ_TIMEOUT = 20; // The maximum time to wait before considering a packet complete

  static const uint8_t MIN_TEMPERATURE = 16; // Minimum temperature as reported by Panasonic app
  static const uint8_t MAX_TEMPERATURE = 30; // Maximum temperature as supported by Panasonic app
  static const float TEMPERATURE_STEP = 0.5; // Steps the temperature can be set in
  static const float TEMPERATURE_TOLERANCE = 2; // The tolerance to allow when checking the climate state
  static const uint8_t TEMPERATURE_THRESHOLD = 100; // Maximum temperature the AC can report before considering the temperature as invalid

  enum class CommandType
  {
    Normal,
    Response,
    Resend
  };

  enum class ACType
  {
    DNSKP11, // New module (via CN-WLAN)
    CZTACG1 // Old module (via CN-CNT)
  };

  class PanasonicAC : public Component, public uart::UARTDevice, public climate::Climate
  {
    public:
      PanasonicAC(uart::UARTComponent *parent);

      void control(const climate::ClimateCall &call) override;

      virtual void set_outside_temperature_sensor(sensor::Sensor *outside_temperature_sensor);
      virtual void set_vertical_swing_sensor(text_sensor::TextSensor *vertical_swing_sensor);
      virtual void set_horizontal_swing_sensor(text_sensor::TextSensor *horizontal_swing_sensor);
      virtual void set_nanoex_switch(switch_::Switch *nanoex_switch);

      void setup() override;
      void loop() override;

    protected:
      sensor::Sensor *outside_temperature_sensor = NULL; // Sensor to store outside temperature from queries
      text_sensor::TextSensor *vertical_swing_sensor = NULL; // Text sensor to store manual position of vertical swing
      text_sensor::TextSensor *horizontal_swing_sensor = NULL; // Text sensor to store manual position of horizontal swing
      switch_::Switch *nanoex_switch = NULL; // Switch to toggle nanoeX on/off
      std::string vertical_swing_state;
      std::string horizontal_swing_state;
      bool nanoex_state = false; // Stores the state of nanoex to prevent duplicate packets

      bool waitingForResponse = false; // Set to true if we are waiting for a response

      int receiveBufferIndex = 0; // Current position of the receive buffer
      byte receiveBuffer[ESPPAC::BUFFER_SIZE]; // Stores the packet currently being received

      unsigned long initTime; // Stores the current time
      unsigned long lastRead; // Stores the time at which the last read was done
      unsigned long lastPacketSent; // Stores the time at which the last packet was sent
      unsigned long lastPacketReceived; // Stores the time at which the last packet was received

      climate::ClimateTraits traits() override;

      void read_data();

      void update_outside_temperature(int8_t temperature);
      void update_current_temperature(int8_t temperature);
      void update_target_temperature(int8_t temperature);
      void update_swing_horizontal(const char* swing);
      void update_swing_vertical(const char* swing);
      void update_nanoex(bool nanoex);

      climate::ClimateAction determine_action();

      void log_packet(byte array[], size_t length, bool outgoing = false);
  };
}
