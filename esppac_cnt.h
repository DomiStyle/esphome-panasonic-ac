#include "esphome.h"
#include "esppac.h"

namespace ESPPAC
{
  namespace CNT
  {
    static const ACType TYPE = ACType::CZTACG1;

    static const byte CTRL_HEADER = 0xF0; // The header for control frames
    static const byte POLL_HEADER = 0x70; // The header for the poll command

    static const int POLL_INTERVAL = 5000; // The interval at which to poll the AC

    enum class ACState
    {
      Initializing, // Before first query response is receive
      Ready, // All done, ready to receive regular packets
    };

    class PanasonicACCNT : public ESPPAC::PanasonicAC
    {
      public:
        PanasonicACCNT(uart::UARTComponent *uartComponent);

        void control(const climate::ClimateCall &call) override;

        //void set_vertical_swing_sensor(text_sensor::TextSensor *vertical_swing_sensor) override;
        //void set_horizontal_swing_sensor(text_sensor::TextSensor *horizontal_swing_sensor) override;
        void set_nanoex_switch(switch_::Switch *nanoex_switch) override;
        void set_eco_switch(switch_::Switch *eco_switch) override;
        void set_mild_dry_switch(switch_::Switch *mild_dry_switch) override;

        void setup() override;
        void loop() override;

      private:
        ACState state = ACState::Initializing; // Stores the internal state of the AC, used during initialization

        byte data[10]; // Stores the data received from the AC
        void handle_poll();

        void set_data(bool set);

        void send_command(const byte* command, size_t commandLength, CommandType type, byte header);
        void send_packet(byte* packet, size_t packetLength, CommandType type);

        bool verify_packet();
        void handle_packet();

        climate::ClimateMode determine_mode(byte mode);
        //std::string determine_fan_speed(byte speed);
        climate::ClimateFanMode determine_fan_speed(byte speed);

        const char* determine_vertical_swing(byte swing);
        const char* determine_horizontal_swing(byte swing);

        std::string determine_preset(byte preset);
        bool determine_preset_nanoex(byte preset);
        bool determine_eco(byte value);
        bool determine_mild_dry(byte value);
    };
  }
}
