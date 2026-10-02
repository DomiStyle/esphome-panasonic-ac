#include "esphome/components/climate/climate.h"
#include "esphome/components/climate/climate_mode.h"
#include "esppac.h"
#include "esppac_wlan_types.h"

namespace esphome {
namespace panasonic_ac {
namespace WLAN {

#ifndef PANASONIC_AC_NUM_EXTRA_POLL_PROPERTIES
#define PANASONIC_AC_NUM_EXTRA_POLL_PROPERTIES 0
#endif

static const uint8_t HEADER = 0x5A;  // The header of the protocol, every packet starts with this

static const int INIT_TIMEOUT = 10000;       // Time to wait before initializing after boot
static const int INIT_END_TIMEOUT = 10000;   // Time to wait for last handshake packet
static const int FIRST_POLL_TIMEOUT = 650;   // Time to wait before requesting the first poll
static const int POLL_INTERVAL = 30000;      // The interval at which to poll the AC
static const int RESPONSE_TIMEOUT = 600;     // The timeout after which we expect a response to our last command
static const int INIT_FAIL_TIMEOUT = 30000;  // The timeout after which the initialization is considered failed
static const size_t TX_BUFFER_SIZE = 128;    // The maximum length for outgoing packets used for the packet buffer

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

  void on_horizontal_swing_change(const StringRef &swing) override;
  void on_vertical_swing_change(const StringRef &swing) override;
  void on_nanoex_change(bool nanoex) override;
  void on_eco_change(bool eco) override;
  void on_econavi_change(bool eco) override;
  void on_mild_dry_change(bool mild_dry) override;

  void setup() override;
  void loop() override;

  void set_extra_poll_properties(std::initializer_list<PollProperty> properties) {
    init_array_from(this->extra_poll_properties_, properties);
  }
  void set_poll_properties_mode(PollPropertiesMode mode) { this->poll_properties_mode_ = mode; }

 protected:
  ACState state_ = ACState::Initializing;  // Stores the internal state of the AC, used during initialization

  uint8_t transmit_packet_count_ = 0;  // Counter used in packet (2nd byte) when we are sending packets
  uint8_t receive_packet_count_ = 0;   // Counter used in packet (2nd byte) when AC is sending us packets

  uint8_t tx_buffer_[TX_BUFFER_SIZE];  // Buffer for outgoing packages
  size_t set_queue_index_ = 0;  // Stores the index of the next set property (0 = no property packet currently being built)
  size_t last_sent_length_ = 0;  // length of the last packet actually transmitted, for resend

  std::array<PollProperty, PANASONIC_AC_NUM_EXTRA_POLL_PROPERTIES> extra_poll_properties_{};
  PollPropertiesMode poll_properties_mode_ = PollPropertiesMode::EXTEND;

  void handle_init_packets();
  void handle_handshake_packet();

  void handle_poll();
  bool verify_packet();
  void handle_packet();

  void send_set_command();
  void send_poll();
  void send_command(const uint8_t *command, size_t commandLength, CommandType type = CommandType::Normal);
  void send_packet(CommandType type);

  void handle_resend();

  void start_property_packet(uint8_t msg_type_hi, uint8_t msg_type_lo);
  void set_value(uint8_t property, uint8_t value, uint8_t attrb=0x00);
  void set_value(uint8_t property, const uint8_t *value, size_t length, uint8_t attrb=0x00);
  void request_value(uint8_t property, uint8_t attrb=0x00);

  size_t parse_property(size_t offset, size_t end);
  void decode_properties();
  bool power_state = false;
};

}  // namespace WLAN
}  // namespace panasonic_ac
}  // namespace esphome
