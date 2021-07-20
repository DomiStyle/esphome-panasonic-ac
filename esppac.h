#include "esphome.h"

using namespace esphome;

static const char* ESPPAC_VERSION = "1.2.0";
static const char* TAG = "esppac";

static const byte ESPPAC_HEADER = 0x5A; // The header of the protocol, every packet starts with this
static const int ESPPAC_BUFFER_SIZE = 128; // The maximum size of a single packet (both receive and transmit)

static const int ESPPAC_INIT_TIMEOUT = 10000; // Time to wait before initializing after boot
static const int ESPPAC_INIT_END_TIMEOUT = 10000; // Time to wait for last handshake packet
static const int ESPPAC_FIRST_POLL_TIMEOUT = 650; // Time to wait before requesting the first poll
static const int ESPPAC_READ_TIMEOUT = 20; // The maximum time to wait before considering a packet complete
static const int ESPPAC_RESPONSE_TIMEOUT = 600; // The timeout after which we expect a response to our last command
static const int ESPPAC_INIT_FAIL_TIMEOUT = 30000; // The timeout after which the initialization is considered failed
static const int ESPPAC_POLL_INTERVAL = 55000; // The interval at which to poll the AC

static const uint8_t ESPPAC_MIN_TEMPERATURE = 16; // Minimum temperature as reported by Panasonic app
static const uint8_t ESPPAC_MAX_TEMPERATURE = 30; // Maximum temperature as supported by Panasonic app
static const float ESPPAC_TEMPERATURE_STEP = 0.5; // Steps the temperature can be set in
static const float ESPPAC_TEMPERATURE_TOLERANCE = 2; // The tolerance to allow when checking the climate state
static const uint8_t ESPPAC_TEMPERATURE_THRESHOLD = 100; // Maximum temperature the AC can report before considering the temperature as invalid

enum class CommandType
{
  Normal,
  Response,
  Resend
};

enum class ACState
{
  Initializing, // Before first handshake packet is sent
  Handshake, // During the initial handshake
  FirstPoll, // After the handshake, before polling for the first time
  HandshakeEnding, // After the first poll, waiting for the last handshake packet
  Ready, // All done, ready to receive regular packets
  Failed // Initialization failed
};

class PanasonicAC : public Component, public uart::UARTDevice, public climate::Climate
{
  public:
    PanasonicAC(uart::UARTComponent *parent);

    void control(const climate::ClimateCall &call) override;

    void set_outside_temperature_sensor(sensor::Sensor *outside_temperature_sensor);
    void set_vertical_swing_sensor(text_sensor::TextSensor *vertical_swing_sensor);
    void set_horizontal_swing_sensor(text_sensor::TextSensor *horizontal_swing_sensor);
    void set_nanoex_switch(switch_::Switch *nanoex_switch);

    void setup() override;
    void loop() override;

  protected:
    climate::ClimateTraits traits() override;

  private:
    sensor::Sensor *outside_temperature_sensor = NULL; // Sensor to store outside temperature from queries
    text_sensor::TextSensor *vertical_swing_sensor = NULL; // Text sensor to store manual position of vertical swing
    text_sensor::TextSensor *horizontal_swing_sensor = NULL; // Text sensor to store manual position of horizontal swing
    switch_::Switch *nanoex_switch = NULL; // Switch to toggle nanoeX on/off
    std::string vertical_swing_state;
    std::string horizontal_swing_state;
    bool nanoex_state = false; // Stores the state of nanoex to prevent duplicate packets

    ACState state = ACState::Initializing; // Stores the internal state of the AC, used during initialization
    bool waitingForResponse = false; // Set to true if we are waiting for a response

    byte transmitPacketCount = 0; // Counter used in packet (2nd byte) when we are sending packets
    byte receivePacketCount = 0; // Counter used in packet (2nd byte) when AC is sending us packets

    int receiveBufferIndex = 0; // Current position of the receive buffer
    byte receiveBuffer[ESPPAC_BUFFER_SIZE]; // Stores the packet currently being received

    const byte* lastCommand; // Stores a pointer to the last command we executed
    size_t lastCommandLength; // Stores the length of the last command we executed

    byte setQueue[16][2]; // Queue to store the key/value for the set commands
    byte setQueueIndex = 0; // Stores the index of the next key/value set

    unsigned long initTime; // Stores the current time
    unsigned long lastRead; // Stores the time at which the last read was done
    unsigned long lastPacketSent; // Stores the time at which the last packet was sent
    unsigned long lastPacketReceived; // Stores the time at which the last packet was received

    void handle_init_packets();
    void handle_poll();

    bool verify_packet();
    void handle_packet();
    void handle_handshake_packet();

    void send_set_command();
    void send_command(const byte* command, size_t commandLength, CommandType type = CommandType::Normal);
    void send_packet(byte* packet, size_t packetLength, CommandType type = CommandType::Normal);

    void determine_mode(byte mode);
    void determine_fan_speed(byte speed);
    void determine_fan_power(byte power);
    void determine_swing(byte swing);
    void determine_action();

    void update_outside_temperature(int8_t temperature);
    void update_current_temperature(int8_t temperature);
    void update_target_temperature(int8_t temperature);
    void update_swing_horizontal(byte swing);
    void update_swing_vertical(byte swing);
    void update_nanoex(byte nanoex);

    const char* get_swing_vertical(byte swing);
    const char* get_swing_horizontal(byte swing);

    void read_data();
    void handle_resend();

    void set_value(byte key, byte value);

    void log_packet(byte array[], size_t length, bool outgoing = false);
};
