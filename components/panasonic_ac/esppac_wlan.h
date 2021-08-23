#include "esphome/components/climate/climate.h"
#include "esphome/components/climate/climate_mode.h"
#include "esppac.h"

namespace esphome {
namespace panasonic_ac {
namespace WLAN {

static const uint8_t HEADER = 0x5A;  // The header of the protocol, every packet starts with this

static const int INIT_TIMEOUT = 10000;       // Time to wait before initializing after boot
static const int INIT_END_TIMEOUT = 10000;   // Time to wait for last handshake packet
static const int FIRST_POLL_TIMEOUT = 650;   // Time to wait before requesting the first poll
static const int POLL_INTERVAL = 30000;      // The interval at which to poll the AC
static const int RESPONSE_TIMEOUT = 600;     // The timeout after which we expect a response to our last command
static const int INIT_FAIL_TIMEOUT = 30000;  // The timeout after which the initialization is considered failed

enum class ACState {
  Initializing,     // Before first handshake packet is sent
  Handshake,        // During the initial handshake
  FirstPoll,        // After the handshake, before polling for the first time
  HandshakeEnding,  // After the first poll, waiting for the last handshake packet
  Ready,            // All done, ready to receive regular packets
  Failed            // Initialization failed
};

class PanasonicACWLAN : public PanasonicAC {
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

 protected:
  ACState state_ = ACState::Initializing;  // Stores the internal state of the AC, used during initialization

  uint8_t transmit_packet_count_ = 0;  // Counter used in packet (2nd byte) when we are sending packets
  uint8_t receive_packet_count_ = 0;   // Counter used in packet (2nd byte) when AC is sending us packets

  const uint8_t *last_command_;  // Stores a pointer to the last command we executed
  size_t last_command_length_;   // Stores the length of the last command we executed

  uint8_t set_queue_[16][2];     // Queue to store the key/value for the set commands
  uint8_t set_queue_index_ = 0;  // Stores the index of the next key/value set

  void handle_init_packets();
  void handle_handshake_packet();

  void handle_poll();
  bool verify_packet();
  void handle_packet();

  void send_set_command();
  void send_command(const uint8_t *command, size_t commandLength, CommandType type = CommandType::Normal);
  void send_packet(std::vector<uint8_t> packet, CommandType type = CommandType::Normal);

  climate::ClimateMode determine_mode(uint8_t mode);
  std::string determine_fan_speed(uint8_t speed);
  std::string determine_preset(uint8_t preset);
  std::string determine_swing_vertical(uint8_t swing);
  std::string determine_swing_horizontal(uint8_t swing);
  climate::ClimateSwingMode determine_swing(uint8_t swing);
  bool determine_nanoex(uint8_t nanoex);

  void handle_resend();

  void set_value(uint8_t key, uint8_t value);
};

}  // namespace WLAN
}  // namespace panasonic_ac
}  // namespace esphome
