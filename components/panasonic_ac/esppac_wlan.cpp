#include "esppac_wlan.h"
#include "esppac_commands_wlan.h"

#include "esphome/core/log.h"

namespace esphome {
namespace panasonic_ac {
namespace WLAN {

static const char *const TAG = "panasonic_ac.dnskp11";

void PanasonicACWLAN::setup() {
  PanasonicAC::setup();

  ESP_LOGD(TAG, "Using DNSK-P11 protocol via CN-WLAN");
}

void PanasonicACWLAN::loop() {
  if (this->state_ != ACState::Ready) {
    handle_init_packets();  // Handle initialization packets separate from normal packets

    if (millis() - this->init_time_ > INIT_FAIL_TIMEOUT) {
      this->state_ = ACState::Failed;
      mark_failed();
      return;
    }
  }

  if (millis() - this->last_read_ > READ_TIMEOUT &&
      !this->rx_buffer_.empty())  // Check if our read timed out and we received something
  {
    log_packet(this->rx_buffer_);

    // Check for defrost status packet
    if (this->rx_buffer_[0] == 0x70 && this->rx_buffer_.size() >= 15) {
      bool defrost = (this->rx_buffer_[14] == 0x02);
      update_defrost(defrost);
      this->rx_buffer_.clear();
      return;
    }

    if (!verify_packet())  // Verify length, header, counter and checksum
      return;

    this->waiting_for_response_ =
        false;  // Set that we are not waiting for a response anymore since we received a valid one
    this->last_packet_received_ = millis();  // Set the time at which we received our last packet

    if (this->state_ == ACState::Ready || this->state_ == ACState::FirstPoll ||
        this->state_ == ACState::HandshakeEnding)  // Parse regular packets
    {
      handle_packet();  // Handle regular packet
    } else              // Parse handshake packets
    {
      handle_handshake_packet();  // Not initialized yet, handle handshake packet
    }

    this->rx_buffer_.clear();  // Reset buffer
  }

  PanasonicAC::read_data();

  handle_resend();  // Handle packets that need to be resent

  handle_poll();  // Handle sending poll packets
}

/*
 * ESPHome control request
 */

void PanasonicACWLAN::control(const climate::ClimateCall &call) {
  if (this->state_ != ACState::Ready)
    return;

  if (call.get_mode().has_value()) {
    ESP_LOGV(TAG, "Requested mode change");

    switch (*call.get_mode()) {
      case climate::CLIMATE_MODE_COOL:
        set_value(0xB0, 0x42);
        set_value(0x80, 0x30);
        break;
      case climate::CLIMATE_MODE_HEAT:
        set_value(0xB0, 0x43);
        set_value(0x80, 0x30);
        break;
      case climate::CLIMATE_MODE_DRY:
        set_value(0xB0, 0x44);
        set_value(0x80, 0x30);
        break;
      case climate::CLIMATE_MODE_HEAT_COOL:
        set_value(0xB0, 0x41);
        set_value(0x80, 0x30);
        break;
      case climate::CLIMATE_MODE_FAN_ONLY:
        set_value(0xB0, 0x45);
        set_value(0x80, 0x30);
        break;
      case climate::CLIMATE_MODE_OFF:
        set_value(0x80, 0x31);
        break;
      default:
        ESP_LOGV(TAG, "Unsupported mode requested");
        break;
    }

    this->mode =
        *call.get_mode();   // Set mode manually since we won't receive a report from the AC if its the same mode again
    this->publish_state();  // Send this state, will get updated once next poll is executed
  }

  if (call.get_target_temperature().has_value()) {
    ESP_LOGV(TAG, "Requested target temp change to %.2f, %.2f including offset", *call.get_target_temperature(), *call.get_target_temperature() - this->current_temperature_offset_);
    set_value(0x31, (*call.get_target_temperature() - this->current_temperature_offset_) * 2);
  }

  if (call.has_custom_fan_mode()) {
    ESP_LOGV(TAG, "Requested fan mode change");

    const StringRef fanMode = call.get_custom_fan_mode();

    if (fanMode == "Automatic") {
      set_value(0xB2, 0x41);
      set_value(0xA0, 0x41);
    } else if (fanMode == "1") {
      set_value(0xB2, 0x41);
      set_value(0xA0, 0x32);
    } else if (fanMode == "2") {
      set_value(0xB2, 0x41);
      set_value(0xA0, 0x33);
    } else if (fanMode == "3") {
      set_value(0xB2, 0x41);
      set_value(0xA0, 0x34);
    } else if (fanMode == "4") {
      set_value(0xB2, 0x41);
      set_value(0xA0, 0x35);
    } else if (fanMode == "5") {
      set_value(0xB2, 0x41);
      set_value(0xA0, 0x36);
    } else
      ESP_LOGV(TAG, "Unsupported fan mode requested");
  }

  if (call.get_swing_mode().has_value()) {
    ESP_LOGV(TAG, "Requested swing mode change");

    switch (*call.get_swing_mode()) {
      case climate::CLIMATE_SWING_BOTH:
        set_value(0xA1, 0x41);
        break;
      case climate::CLIMATE_SWING_OFF:
        set_value(0xA1, 0x42);
        set_value(0xA4, 0x43);
        set_value(0xA5, 0x43);
        set_value(0x35, 0x42);
        break;
      case climate::CLIMATE_SWING_VERTICAL:
        set_value(0xA1, 0x43);
        set_value(0xA5, 0x43);
        break;
      case climate::CLIMATE_SWING_HORIZONTAL:
        set_value(0xA1, 0x44);
        set_value(0xA4, 0x43);
        break;
      default:
        ESP_LOGV(TAG, "Unsupported swing mode requested");
        break;
    }
  }

  if (call.has_custom_preset()) {
    ESP_LOGV(TAG, "Requested preset change");

    const StringRef preset = call.get_custom_preset();

    if (preset == "Normal") {
      set_value(0xB2, 0x41);
      set_value(0x35, 0x42);
      set_value(0x34, 0x42);
    } else if (preset == "Powerful") {
      set_value(0xB2, 0x42);
      set_value(0x35, 0x42);
      set_value(0x34, 0x42);
    } else if (preset == "Quiet") {
      set_value(0xB2, 0x43);
      set_value(0x35, 0x42);
      set_value(0x34, 0x42);
    } else
      ESP_LOGV(TAG, "Unsupported preset requested");
  }

  if (this->set_queue_index_ > 0)  // Only send packet if any changes need to be made
  {
    send_set_command();
  }
}

/*
 * Loop handling
 */

void PanasonicACWLAN::handle_poll() {
  if (this->state_ == ACState::Ready && millis() - this->last_packet_sent_ > POLL_INTERVAL) {
    ESP_LOGV(TAG, "Polling AC");
    this->send_poll();
  }
}

void PanasonicACWLAN::handle_init_packets() {
  if (this->state_ == ACState::Initializing) {
    if (millis() - this->init_time_ > INIT_TIMEOUT)  // Handle handshake initialization
    {
      ESP_LOGD(TAG, "Starting handshake [1/16]");
      send_command(CMD_HANDSHAKE_1,
                   sizeof(CMD_HANDSHAKE_1));  // Send first handshake packet, AC won't send a response
      delay(3);                               // Add small delay to mimic real wifi adapter
      send_command(CMD_HANDSHAKE_2,
                   sizeof(CMD_HANDSHAKE_2));  // Send second handshake packet, AC won't send a response
                                              // but we will trigger a resend

      this->state_ = ACState::Handshake;  // Update state to handshake started
    }
  } else if (this->state_ == ACState::FirstPoll &&
             millis() - this->last_packet_sent_ > FIRST_POLL_TIMEOUT)  // Handle sending first poll
  {
    ESP_LOGD(TAG, "Polling for the first time");
    this->send_poll();

    this->state_ = ACState::HandshakeEnding;
  } else if (this->state_ == ACState::HandshakeEnding &&
             millis() - this->last_packet_sent_ > INIT_END_TIMEOUT)  // Handle last handshake message
  {
    ESP_LOGD(TAG, "Finishing handshake [16/16]");
    send_command(CMD_HANDSHAKE_16, sizeof(CMD_HANDSHAKE_16));

    // State is set to ready in the response to this packet
  }
}

bool PanasonicACWLAN::verify_packet() {
  if (this->rx_buffer_.size() < 5)  // Drop packets that are too short
  {
    ESP_LOGW(TAG, "Dropping invalid packet (length)");
    this->rx_buffer_.clear();  // Reset buffer
    return false;
  }

  if (this->rx_buffer_[0] == 0x66)  // Sync packets are the only packet not starting with 0x5A
  {
    ESP_LOGI(TAG, "Received sync packet, triggering initialization");
    this->init_time_ -= INIT_TIMEOUT;  // Set init time back to trigger a initialization now
    this->rx_buffer_.clear();          // Reset buffer
    return false;
  }

  if (this->rx_buffer_[0] != HEADER)  // Check if header matches
  {
    ESP_LOGW(TAG, "Dropping invalid packet (header)");
    this->rx_buffer_.clear();  // Reset buffer
    return false;
  }

  if (this->state_ == ACState::Ready && this->waiting_for_response_)  // If we were waiting for a response, check if the
                                                                      // tx packet counter matches (if we are ready)
  {
    if (this->rx_buffer_[1] != this->transmit_packet_count_ - 1 &&
        this->rx_buffer_[1] != 0xFE)  // Check transmit packet counter
    {
      ESP_LOGW(TAG, "Correcting shifted tx counter");
      this->receive_packet_count_ = this->rx_buffer_[1];
    }
  } else if (this->state_ == ACState::Ready)  // If we were not waiting for a response, check if the rx packet counter
                                              // matches (if we are ready)
  {
    if (this->rx_buffer_[1] != this->receive_packet_count_)  // Check receive packet counter
    {
      ESP_LOGW(TAG, "Correcting shifted rx counter");
      this->receive_packet_count_ = this->rx_buffer_[1];
    }
  }

  uint8_t checksum = 0;  // Set checksum to first byte

  for (uint8_t i : this->rx_buffer_)  // Add all bytes together
    checksum += i;

  if (checksum != 0)  // Check if checksum is valid
  {
    ESP_LOGD(TAG, "Dropping invalid packet (checksum)");

    this->rx_buffer_.clear();  // Reset buffer
    return false;
  }

  return true;
}

/*
 * Field handling
 */

static climate::ClimateMode determine_mode(uint8_t mode) {
  switch (mode)  // Check mode
  {
    case 0x41:  // Auto
      return climate::CLIMATE_MODE_HEAT_COOL;
    case 0x42:  // Cool
      return climate::CLIMATE_MODE_COOL;
    case 0x43:  // Heat
      return climate::CLIMATE_MODE_HEAT;
    case 0x44:  // Dry
      return climate::CLIMATE_MODE_DRY;
    case 0x45:  // Fan only?
      return climate::CLIMATE_MODE_FAN_ONLY;
    default:
      ESP_LOGW(TAG, "Received unknown climate mode (0x%02X)", mode);
      return climate::CLIMATE_MODE_OFF;
  }
}

static const char *determine_fan_speed(uint8_t speed) {
  switch (speed) {
    case 0x32:  // 1
      return "1";
    case 0x33:  // 2
      return "2";
    case 0x34:  // 3
      return "3";
    case 0x35:  // 4
      return "4";
    case 0x36:  // 5
      return "5";
    case 0x41:  // Auto
      return "Automatic";
    default:
      ESP_LOGW(TAG, "Received unknown fan speed (0x%02X)", speed);
      return "Unknown";
  }
}

static const char *determine_preset(uint8_t preset) {
  switch (preset) {
    case 0x43:  // Quiet
      return "Quiet";
    case 0x42:  // Powerful
      return "Powerful";
    case 0x41:  // Normal
      return "Normal";
    default:
      ESP_LOGW(TAG, "Received unknown preset (0x%02X)", preset);
      return "Normal";
  }
}

static const char *determine_swing_vertical(uint8_t swing) {
  switch (swing) {
    case 0x42:  // Down
      return "down";
    case 0x45:  // Down center
      return "down_center";
    case 0x43:  // Center
      return "center";
    case 0x44:  // Up Center
      return "up_center";
    case 0x41:  // Up
      return "up";
    default:
      ESP_LOGW(TAG, "Received unknown vertical swing position (0x%02X)", swing);
      return "Unknown";
  }
}

static const char *determine_swing_horizontal(uint8_t swing) {
  switch (swing) {
    case 0x42:  // Left
      return "left";
    case 0x5C:  // Left center
      return "left_center";
    case 0x43:  // Center
      return "center";
    case 0x56:  // Right center
      return "right_center";
    case 0x41:  // Right
      return "right";
    default:
      ESP_LOGW(TAG, "Received unknown horizontal swing position (0x%02X)", swing);
      return "Unknown";
  }
}

static climate::ClimateSwingMode determine_swing(uint8_t swing) {
  switch (swing) {
    case 0x41:  // Both
      return climate::CLIMATE_SWING_BOTH;
    case 0x42:  // Off
      return climate::CLIMATE_SWING_OFF;
    case 0x43:  // Vertical
      return climate::CLIMATE_SWING_VERTICAL;
    case 0x44:  // Horizontal
      return climate::CLIMATE_SWING_HORIZONTAL;
    default:
      ESP_LOGW(TAG, "Received unknown swing mode (0x%02X)", swing);
      return climate::CLIMATE_SWING_OFF;
  }
}

static constexpr bool determine_nanoex(uint8_t nanoex) {
  switch (nanoex) {
    case 0x42:
      return false;
    default:
      return true;
  }
}

/*
 * Packet handling
 */

void PanasonicACWLAN::handle_packet() {
  if (this->rx_buffer_[2] == 0x01 && this->rx_buffer_[3] == 0x01)  // Ping
  {
    ESP_LOGD(TAG, "Answering ping");
    send_command(CMD_PING, sizeof(CMD_PING), CommandType::Response);
  } else if (this->rx_buffer_[2] == 0x10 && this->rx_buffer_[3] == 0x89)  // Received query response
  {
    ESP_LOGD(TAG, "Received query response");

    if (this->rx_buffer_.size() < 12) {
      ESP_LOGW(TAG, "Received too short query response (size: %d)", this->rx_buffer_.size());
      return;
    }
    this->decode_properties();
  } else if (this->rx_buffer_[2] == 0x10 && this->rx_buffer_[3] == 0x88)  // Command ack
  {
    ESP_LOGV(TAG, "Received command ack");
  } else if (this->rx_buffer_[2] == 0x10 && this->rx_buffer_[3] == 0x0A)  // Report
  {
    ESP_LOGV(TAG, "Received report");
    send_command(CMD_REPORT_ACK, sizeof(CMD_REPORT_ACK), CommandType::Response);

    if (this->rx_buffer_.size() < 12) {
      ESP_LOGE(TAG, "Report is too short to handle (size: %d)", this->rx_buffer_.size());
      return;
    }
    this->decode_properties();
  } else if (this->rx_buffer_[2] == 0x01 && this->rx_buffer_[3] == 0x80)  // Answer for handshake 16
  {
    ESP_LOGI(TAG, "Panasonic AC component v%s initialized", VERSION);
    this->state_ = ACState::Ready;
  } else {
    ESP_LOGW(TAG, "Received unknown packet");
  }
}

void PanasonicACWLAN::handle_handshake_packet() {
  if (this->rx_buffer_[2] == 0x00 && this->rx_buffer_[3] == 0x89)  // Answer for handshake 2
  {
    ESP_LOGD(TAG, "Answering handshake [2/16]");
    send_command(CMD_HANDSHAKE_3, sizeof(CMD_HANDSHAKE_3));
  } else if (this->rx_buffer_[2] == 0x00 && this->rx_buffer_[3] == 0x8C)  // Answer for handshake 3
  {
    ESP_LOGD(TAG, "Answering handshake [3/16]");
    send_command(CMD_HANDSHAKE_4, sizeof(CMD_HANDSHAKE_4));
  } else if (this->rx_buffer_[2] == 0x00 && this->rx_buffer_[3] == 0x90)  // Answer for handshake 4
  {
    ESP_LOGD(TAG, "Answering handshake [4/16]");
    send_command(CMD_HANDSHAKE_5, sizeof(CMD_HANDSHAKE_5));
  } else if (this->rx_buffer_[2] == 0x00 && this->rx_buffer_[3] == 0x91)  // Answer for handshake 5
  {
    ESP_LOGD(TAG, "Answering handshake [5/16]");
    send_command(CMD_HANDSHAKE_6, sizeof(CMD_HANDSHAKE_6));
  } else if (this->rx_buffer_[2] == 0x00 && this->rx_buffer_[3] == 0x92)  // Answer for handshake 6
  {
    ESP_LOGD(TAG, "Answering handshake [6/16]");
    send_command(CMD_HANDSHAKE_7, sizeof(CMD_HANDSHAKE_7));
  } else if (this->rx_buffer_[2] == 0x00 && this->rx_buffer_[3] == 0xC1)  // Answer for handshake 7
  {
    ESP_LOGD(TAG, "Answering handshake [7/16]");
    send_command(CMD_HANDSHAKE_8, sizeof(CMD_HANDSHAKE_8));
  } else if (this->rx_buffer_[2] == 0x01 && this->rx_buffer_[3] == 0xCC)  // Answer for handshake 8
  {
    ESP_LOGD(TAG, "Answering handshake [8/16]");
    send_command(CMD_HANDSHAKE_9, sizeof(CMD_HANDSHAKE_9));
  } else if (this->rx_buffer_[2] == 0x10 && this->rx_buffer_[3] == 0x80)  // Answer for handshake 9
  {
    ESP_LOGD(TAG, "Answering handshake [9/16]");
    send_command(CMD_HANDSHAKE_10, sizeof(CMD_HANDSHAKE_10));
  } else if (this->rx_buffer_[2] == 0x10 && this->rx_buffer_[3] == 0x81)  // Answer for handshake 10
  {
    ESP_LOGD(TAG, "Answering handshake [10/16]");
    send_command(CMD_HANDSHAKE_11, sizeof(CMD_HANDSHAKE_11));
  } else if (this->rx_buffer_[2] == 0x00 && this->rx_buffer_[3] == 0x98)  // Answer for handshake 11
  {
    ESP_LOGD(TAG, "Answering handshake [11/16]");
    send_command(CMD_HANDSHAKE_12, sizeof(CMD_HANDSHAKE_12));
  } else if (this->rx_buffer_[2] == 0x01 && this->rx_buffer_[3] == 0x80)  // Answer for handshake 12
  {
    ESP_LOGD(TAG, "Answering handshake [12/16]");
    send_command(CMD_HANDSHAKE_13, sizeof(CMD_HANDSHAKE_13));
  } else if (this->rx_buffer_[2] == 0x10 && this->rx_buffer_[3] == 0x88)  // Answer for handshake 13
  {
    // Ignore
    ESP_LOGD(TAG, "Ignoring handshake [13/16]");
  } else if (this->rx_buffer_[2] == 0x01 &&
             this->rx_buffer_[3] == 0x09)  // First unsolicited packet from AC containing rx counter
  {
    ESP_LOGD(TAG, "Received rx counter [14/16]");
    this->receive_packet_count_ = this->rx_buffer_[1];  // Set rx packet counter
    send_command(CMD_HANDSHAKE_14, sizeof(CMD_HANDSHAKE_14), CommandType::Response);
  } else if (this->rx_buffer_[2] == 0x00 && this->rx_buffer_[3] == 0x20)  // Second unsolicited packet from AC
  {
    ESP_LOGD(TAG, "Answering handshake [15/16]");
    this->state_ = ACState::FirstPoll;  // Start delayed first poll
    send_command(CMD_HANDSHAKE_15, sizeof(CMD_HANDSHAKE_15), CommandType::Response);
  } else {
    ESP_LOGW(TAG, "Received unknown packet during initialization");
  }
}

/*
 * Packet sending
 */

void PanasonicACWLAN::send_set_command() {
  if (this->set_queue_index_ == 0) return;  // nothing queued
  this->send_packet(CommandType::Normal);
}

void PanasonicACWLAN::send_poll() {
  this->start_property_packet(0x10, 0x09);
  if (this->poll_properties_mode_ == PollPropertiesMode::EXTEND) {
    for (const auto &p : POLL_PROPERTIES) {
      this->request_value(p.property, p.attrb);
    }
  }
  for (const auto &p : this->extra_poll_properties_) {
    this->request_value(p.property, p.attrb);
  }
  this->send_set_command();
}

void PanasonicACWLAN::send_command(const uint8_t *command, size_t commandLength, CommandType type) {
  if ((commandLength + 3) > sizeof(this->tx_buffer_)) {
    ESP_LOGE(TAG, "Failed to create packet from command: command size (%d) exceeds packet capacity (%d).", commandLength, sizeof(this->tx_buffer_));
    return;
  }
  memcpy(&this->tx_buffer_[2], command, commandLength);
  this->set_queue_index_ = commandLength + 2;
  this->send_packet(type);
}

void PanasonicACWLAN::send_packet(CommandType type) {
  uint8_t length = this->set_queue_index_ + 1;

  this->tx_buffer_[0] = HEADER;    // Write header to packet

  uint8_t packetCount = this->transmit_packet_count_;  // Set packet counter
  if (type == CommandType::Response)
    packetCount = this->receive_packet_count_;  // Set the packet counter to the rx counter
  this->tx_buffer_[1] = packetCount;  // Write to packet

  uint8_t checksum = 0;  // Checksum is calculated by adding all bytes together
  for (size_t i = 0; i < length - 1; i++) { // Loop through payload to calculate checksum
    checksum += this->tx_buffer_[i];  // Add byte to checksum
  }
  this->tx_buffer_[length - 1] = (~checksum + 1);     // Compute checksum

  if (type == CommandType::Normal)  // Do not increase tx counter if this was a response or if this was a resent packet
  {
    if (this->transmit_packet_count_ == 0xFE)
      this->transmit_packet_count_ = 0x01;  // Special case, roll over transmit counter after 0xFE
    else
      this->transmit_packet_count_++;  // Increase tx packet counter if this wasn't a response
  } else if (type == CommandType::Response) {
    if (this->receive_packet_count_ == 0xFE)
      this->receive_packet_count_ = 0x01;  // Special case, roll over receive counter after 0xFE
    else
      this->receive_packet_count_++;  // Increase rx counter if this was a response
  }

  if (type != CommandType::Response)     // Don't wait for a response for responses
    this->waiting_for_response_ = true;  // Mark that we are waiting for a response

  this->last_sent_length_ = length;  // remember what actually went out, for resend
  write_array(this->tx_buffer_, length);       // Write to UART
  this->last_packet_sent_ = millis();  // Save the time when we sent the last packet
  this->set_queue_index_ = 0;
}

/*
 * Helpers
 */
void PanasonicACWLAN::handle_resend() {
  if (this->waiting_for_response_ && millis() - this->last_packet_sent_ > RESPONSE_TIMEOUT &&
      this->rx_buffer_.empty())  // Check if AC failed to respond in time and resend packet, if nothing was received yet
  {
    ESP_LOGD(TAG, "Resending previous packet");
    this->write_array(this->tx_buffer_, this->last_sent_length_);
    this->last_packet_sent_ = millis();
  }
}

void PanasonicACWLAN::start_property_packet(uint8_t msg_type_hi, uint8_t msg_type_lo) {
  // Packet structure
  // HEADER(1) SEQUENCE(1) MESSAGE_TYPE(2) PAYLOAD_LENGTH(2) PAYLOAD(PAYLOAD_LENGTH) CHECKSUM(1)
  // Payload structure
  // PAYLOAD_ATTRIBUTE(1) OBJECT_ID(3) PROPERTY_COUNT(1) PROPERTY_LIST
  // Property structure
  // PROPERTY_ATTRIBUTE(1) PROPERTY_ID(1) VALUE_LENGTH(1) VALUE(VALUE_LENGTH)
  // Message type
  this->tx_buffer_[2] = msg_type_hi;
  this->tx_buffer_[3] = msg_type_lo;
  // payload length
  this->tx_buffer_[4] = 0;
  this->tx_buffer_[5] = 0;
  // payload attribute
  this->tx_buffer_[6] = 0x01;
  // property list id
  this->tx_buffer_[7] = 0x01;
  this->tx_buffer_[8] = 0x30;
  this->tx_buffer_[9] = 0x01;
  // property count
  this->tx_buffer_[10] = 0;
  this->set_queue_index_ = 11;
}

void PanasonicACWLAN::set_value(uint8_t property, uint8_t value, uint8_t attrb) {
  this->set_value(property, &value, 1, attrb);
}

void PanasonicACWLAN::set_value(uint8_t property, const uint8_t *value, size_t length, uint8_t attrb) {
  if (this->set_queue_index_ == 0) {  // Starting a fresh property-based packet
    this->start_property_packet(0x10, 0x08);
  }
  if ((this->set_queue_index_ + 3 + length) >= sizeof(this->tx_buffer_)) {
    ESP_LOGE(TAG, "Adding property 0x%02X failed. No space left in tx buffer.", property);
    return;
  }
  this->tx_buffer_[this->set_queue_index_++] = attrb;  // property attribute
  this->tx_buffer_[this->set_queue_index_++] = property;  // property id
  this->tx_buffer_[this->set_queue_index_++] = length;  // length
  if (length > 0) {
    memcpy(&(this->tx_buffer_[this->set_queue_index_]), value, length); //value
  }
  this->set_queue_index_ += length;
  uint16_t payload_length = this->set_queue_index_ - 6;
  this->tx_buffer_[4] = static_cast<uint8_t>(payload_length >> 8);
  this->tx_buffer_[5] = static_cast<uint8_t>(payload_length & 0xFF);
  this->tx_buffer_[10]++;  // property count
}

void PanasonicACWLAN::request_value(uint8_t property, uint8_t attrb) {
  this->set_value(property, nullptr, 0, attrb);
}

/*
 * Sensor handling
 */

void PanasonicACWLAN::on_vertical_swing_change(const StringRef& swing) {
  if (this->state_ != ACState::Ready)
    return;

  ESP_LOGD(TAG, "Setting vertical swing position");

  if (swing == "down")
    set_value(0xA4, 0x42);
  else if (swing == "down_center")
    set_value(0xA4, 0x45);
  else if (swing == "center")
    set_value(0xA4, 0x43);
  else if (swing == "up_center")
    set_value(0xA4, 0x44);
  else if (swing == "up")
    set_value(0xA4, 0x41);

  send_set_command();
}

void PanasonicACWLAN::on_horizontal_swing_change(const StringRef &swing) {
  if (this->state_ != ACState::Ready)
    return;

  ESP_LOGD(TAG, "Setting horizontal swing position");

  if (swing == "left")
    set_value(0xA5, 0x42);
  else if (swing == "left_center")
    set_value(0xA5, 0x5C);
  else if (swing == "center")
    set_value(0xA5, 0x43);
  else if (swing == "right_center")
    set_value(0xA5, 0x56);
  else if (swing == "right")
    set_value(0xA5, 0x41);

  send_set_command();
}

void PanasonicACWLAN::on_nanoex_change(bool state) {
  if (this->state_ != ACState::Ready)
    return;

  if (state) {
    ESP_LOGV(TAG, "Turning nanoex on");
    set_value(0x33, 0x45);  // nanoeX on
  } else {
    ESP_LOGV(TAG, "Turning nanoex off");
    set_value(0x33, 0x42);  // nanoeX off
  }

  send_set_command();
}

void PanasonicACWLAN::on_eco_change(bool state) {
  if (this->state_ != ACState::Ready)
    return;

  return;

  // TODO: implement eco

  // if (state) {
  //   ESP_LOGV(TAG, "Turning eco on");
  //   set_value(..., ...);  // eco on
  // } else {
  //   ESP_LOGV(TAG, "Turning eco off");
  //   set_value(..., ...);  // eco off
  // }

  // send_set_command();
}

void PanasonicACWLAN::on_econavi_change(bool state) {
  if (this->state_ != ACState::Ready)
    return;

  return;

  // TODO: implement econavi

  // if (state) {
  //   ESP_LOGV(TAG, "Turning econavi on");
  //   set_value(..., ...);  // econavi on
  // } else {
  //   ESP_LOGV(TAG, "Turning econavi off");
  //   set_value(..., ...);  // econavi off
  // }

  // send_set_command();
}

void PanasonicACWLAN::on_mild_dry_change(bool state) {
  if (this->state_ != ACState::Ready)
    return;

  return;

  // TODO: implement mild_dry

  // if (state) {
  //   ESP_LOGV(TAG, "Turning mild_dry on");
  //   set_value(..., ...);  // mild_dry on
  // } else {
  //   ESP_LOGV(TAG, "Turning mild_dry off");
  //   set_value(..., ...);  // mild_dry off
  // }

  // send_set_command();
}

void PanasonicACWLAN::decode_properties() {
    size_t offset = 6 + 5;
    size_t size = rx_buffer_.size();
    size_t end = size > 0 ? size - 1 : 0;
    while ((offset + 3) <= end) {
      offset = parse_property(offset, end);
    }
    if (!this->power_state) this->mode = climate::CLIMATE_MODE_OFF;
    climate::ClimateAction action = determine_action();  // Determine the current action of the AC
    this->action = action;
    this->publish_state();
}

size_t PanasonicACWLAN::parse_property(size_t offset, size_t end) {
  uint8_t attrb = this->rx_buffer_[offset];
  uint8_t property = this->rx_buffer_[offset + 1];
  uint8_t length = this->rx_buffer_[offset + 2];
  if ((offset + 3 + length) > end) {
    ESP_LOGW(TAG, "Property 0x%02X length %u exceeds packet (offset=%u, end=%u)", property, length, offset, end);
    return end;
  }
  const uint8_t *value = (length > 0) ? &this->rx_buffer_[offset + 3] : nullptr;

  switch(property) {
    case 0x80:  // Power mode
      if (length == 1) {
        switch (value[0]) {
          case 0x30:  // Power mode on
            ESP_LOGV(TAG, "Received power mode on");
            this->power_state = true;
            break;
          case 0x31:  // Power mode off
            ESP_LOGV(TAG, "Received power mode off");
            this->power_state = false;
            break;
          default:
            ESP_LOGW(TAG, "Received unknown power mode (0x%02X)", value[0]);
            break;
        }
      }
      break;
    case 0xB0:  // Mode
      if (length == 1) {
        this->mode = determine_mode(value[0]);
      }
      break;
    case 0x31:  // Target temperature (old)
      if (length == 1) {
        ESP_LOGV(TAG, "Received target temperature");
        update_target_temperature(value[0]);
      }
      break;
    case 0xB3:  // Target temperature (echonet)
      if (length == 1) {
        ESP_LOGV(TAG, "Received target temperature (echonet)");
        update_target_temperature(value[0] * 2);
      }
      break;
    case 0xF5:  // Target temperature (new)
      if (length == 1) {
        ESP_LOGV(TAG, "Received target temperature (F5)");
        uint8_t temp_raw = ((value[0] & 0x7f) << 1) | (value[0] >> 7);
        update_target_temperature(temp_raw);
      }
      break;
    case 0xBB:  // Current temperature
      if (length == 1) {
        update_current_temperature((int8_t)value[0]);
      }
      break;
    case 0xBE:  // Outside temperature
      if (length == 1) {
        update_outside_temperature((int8_t)value[0]);
      }
      break;
    case 0xA0:  // Fan speed
      if (length == 1) {
        ESP_LOGV(TAG, "Received fan speed");
        this->set_custom_fan_mode_(determine_fan_speed(value[0]));
      }
      break;
    case 0xB2: // Preset
      if (length == 1) {
        ESP_LOGV(TAG, "Received preset");
        this->set_custom_preset_(determine_preset(value[0]));
      }
      break;
    case 0xA1:  // swing mode
      if (length == 1) {
        ESP_LOGV(TAG, "Received swing mode");
        this->swing_mode = determine_swing(value[0]);
      }
      break;
    case 0xA5:  // Horizontal swing position
      if (length == 1) {
        ESP_LOGV(TAG, "Received horizontal swing position");
        update_swing_horizontal(StringRef(determine_swing_horizontal(value[0])));
      }
      break;
    case 0xA4:  // Vertical swing position
      if (length == 1) {
        ESP_LOGV(TAG, "Received vertical swing position");
        update_swing_vertical(StringRef(determine_swing_vertical(value[0])));
      }
      break;
    case 0x33:  // nanoex mode
      if (length == 1) {
        ESP_LOGV(TAG, "Received nanoex state");
        update_nanoex(determine_nanoex(value[0]));
      }
      break;
    default:
      constexpr size_t MAX_LOG_BYTES = 16;
      char hex[format_hex_pretty_size(MAX_LOG_BYTES)];
      format_hex_pretty_to(hex, sizeof(hex), value, std::min<size_t>(length, MAX_LOG_BYTES));
      ESP_LOGD(TAG,
        "Unknown property: attrb=0x%02X id=0x%02X len=%u value=%s",
        attrb, property, length, hex);
  }
  return offset + 3 + length;
}

}  // namespace WLAN
}  // namespace panasonic_ac
}  // namespace esphome
