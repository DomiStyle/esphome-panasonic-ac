#include "esphome.h"
#include "esppac.h"

namespace ESPPAC
{
  namespace WLAN
  {
    static const ACType TYPE = ACType::DNSKP11;

    enum class ACState
    {
      Initializing, // Before first handshake packet is sent
      Handshake, // During the initial handshake
      FirstPoll, // After the handshake, before polling for the first time
      HandshakeEnding, // After the first poll, waiting for the last handshake packet
      Ready, // All done, ready to receive regular packets
      Failed // Initialization failed
    };

    class PanasonicACWLAN : public ESPPAC::PanasonicAC
    {
      public:
        PanasonicACWLAN(uart::UARTComponent *uartComponent);

        void control(const climate::ClimateCall &call) override;

        void set_vertical_swing_sensor(text_sensor::TextSensor *vertical_swing_sensor) override;
        void set_horizontal_swing_sensor(text_sensor::TextSensor *horizontal_swing_sensor) override;
        void set_nanoex_switch(switch_::Switch *nanoex_switch) override;

        void setup() override;
        void loop() override;

      private:
        ACState state = ACState::Initializing; // Stores the internal state of the AC, used during initialization

        byte transmitPacketCount = 0; // Counter used in packet (2nd byte) when we are sending packets
        byte receivePacketCount = 0; // Counter used in packet (2nd byte) when AC is sending us packets

        const byte* lastCommand; // Stores a pointer to the last command we executed
        size_t lastCommandLength; // Stores the length of the last command we executed

        byte setQueue[16][2]; // Queue to store the key/value for the set commands
        byte setQueueIndex = 0; // Stores the index of the next key/value set

        void handle_init_packets();
        void handle_handshake_packet();

        void handle_poll();
        bool verify_packet();
        void handle_packet();

        void send_set_command();
        void send_command(const byte* command, size_t commandLength, CommandType type = CommandType::Normal);
        void send_packet(byte* packet, size_t packetLength, CommandType type = CommandType::Normal);

        void determine_mode(byte mode);
        void determine_fan_speed(byte speed);
        void determine_fan_power(byte power);
        void determine_swing(byte swing);
        void determine_action();

        void handle_resend();

        void set_value(byte key, byte value);
    };
  }
}
