#include "esppac_cnt.h"
#include "esppac_commands_cnt.h"

namespace esphome {
namespace panasonic_ac {
namespace CNT {

static const char *const TAG = "panasonic_ac.cz_tacg1";

void PanasonicACCNT::setup() {
  PanasonicAC::setup();

  ESP_LOGD(TAG, "Using CZ-TACG1 protocol via CN-CNT");
}

void PanasonicACCNT::loop() {
  PanasonicAC::read_data();

  if (millis() - this->last_read_ > READ_TIMEOUT &&
      !this->rx_buffer_.empty())  // Check if our read timed out and we received something
  {
    log_packet(this->rx_buffer_);

    if (!verify_packet())  // Verify length, header, counter and checksum
      return;

    this->waiting_for_response_ = false;
    this->last_packet_received_ = millis();  // Set the time at which we received our last packet

    handle_packet();

    this->rx_buffer_.clear();  // Reset buffer
  }

  handle_poll();  // Handle sending poll packets
}

/*
 * ESPHome control request
 */

void PanasonicACCNT::control(const climate::ClimateCall &call) {
  if (this->state_ != ACState::Ready)
    return;

  if (call.get_mode().has_value()) {
    ESP_LOGV(TAG, "Requested mode change");

    switch (*call.get_mode()) {
      case climate::CLIMATE_MODE_COOL:
        this->data[0] = 0x34;
        break;
      case climate::CLIMATE_MODE_HEAT:
        this->data[0] = 0x44;
        break;
      case climate::CLIMATE_MODE_DRY:
        this->data[0] = 0x24;
        break;
      case climate::CLIMATE_MODE_HEAT_COOL:
        this->data[0] = 0x04;
        break;
      case climate::CLIMATE_MODE_FAN_ONLY:
        this->data[0] = 0x64;
        break;
      case climate::CLIMATE_MODE_OFF:
        this->data[0] = this->data[0] & 0xF0;  // Strip right nib to turn AC off
        break;
      default:
        ESP_LOGV(TAG, "Unsupported mode requested");
        break;
    }
  }

  if (call.get_target_temperature().has_value()) {
    this->data[1] = *call.get_target_temperature() / TEMPERATURE_STEP;
  }

  if (call.get_custom_fan_mode().has_value()) {
    ESP_LOGV(TAG, "Requested fan mode change");

    if(this->custom_preset != "Normal")
    {
      ESP_LOGV(TAG, "Resetting preset");
      this->data[5] = (this->data[5] & 0xF0);  // Clear right nib for normal mode
    }

    std::string fanMode = *call.get_custom_fan_mode();

    if (fanMode == "Automatic")
      this->data[3] = 0xA0;
    else if (fanMode == "1")
      this->data[3] = 0x30;
    else if (fanMode == "2")
      this->data[3] = 0x40;
    else if (fanMode == "3")
      this->data[3] = 0x50;
    else if (fanMode == "4")
      this->data[3] = 0x60;
    else if (fanMode == "5")
      this->data[3] = 0x70;
    else
      ESP_LOGV(TAG, "Unsupported fan mode requested");
  }

  if (call.get_swing_mode().has_value()) {
    ESP_LOGV(TAG, "Requested swing mode change");

    switch (*call.get_swing_mode()) {
      case climate::CLIMATE_SWING_BOTH:
        this->data[4] = 0xFD;
        break;
      case climate::CLIMATE_SWING_OFF:
        this->data[4] = 0x36;  // Reset both to center
        break;
      case climate::CLIMATE_SWING_VERTICAL:
        this->data[4] = 0xF6;  // Swing vertical, horizontal center
        break;
      case climate::CLIMATE_SWING_HORIZONTAL:
        this->data[4] = 0x3D;  // Swing horizontal, vertical center
        break;
      default:
        ESP_LOGV(TAG, "Unsupported swing mode requested");
        break;
    }
  }

  if (call.get_custom_preset().has_value()) {
    ESP_LOGV(TAG, "Requested preset change");

    std::string preset = *call.get_custom_preset();

    if (preset.compare("Normal") == 0)
      this->data[5] = (this->data[5] & 0xF0);  // Clear right nib for normal mode
    else if (preset.compare("Powerful") == 0)
      this->data[5] = (this->data[5] & 0xF0) + 0x02;  // Clear right nib and set powerful mode
    else if (preset.compare("Quiet") == 0)
      this->data[5] = (this->data[5] & 0xF0) + 0x04;  // Clear right nib and set quiet mode
    else
      ESP_LOGV(TAG, "Unsupported preset requested");
  }

  send_command(this->data, CommandType::Normal, CTRL_HEADER);
}

/*
 * Set the data array to the fields
 */
void PanasonicACCNT::set_data(bool set) {
  this->mode = determine_mode(this->data[0]);
  this->custom_fan_mode = determine_fan_speed(this->data[3]);

  std::string verticalSwing = determine_vertical_swing(this->data[4]);
  std::string horizontalSwing = determine_horizontal_swing(this->data[4]);

  std::string preset = determine_preset(this->data[5]);
  bool nanoex = determine_preset_nanoex(this->data[5]);
  bool eco = determine_eco(this->data[8]);
  bool mildDry = determine_mild_dry(this->data[2]);

  this->update_target_temperature((int8_t) this->data[1]);

  if (set)  // Also set current and outside temperature
  {
    this->update_current_temperature((int8_t) this->rx_buffer_[18]);
    this->update_outside_temperature((int8_t) this->rx_buffer_[19]);
  }

  if (verticalSwing == "auto" && horizontalSwing == "auto")
    this->swing_mode = climate::CLIMATE_SWING_BOTH;
  else if (verticalSwing == "auto")
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
  else if (horizontalSwing == "auto")
    this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
  else
    this->swing_mode = climate::CLIMATE_SWING_OFF;

  this->update_swing_vertical(verticalSwing);
  this->update_swing_horizontal(horizontalSwing);

  this->custom_preset = preset;

  this->update_nanoex(nanoex);
  this->update_eco(eco);
  this->update_mild_dry(mildDry);
}

/*
 * Send a command, attaching header, packet length and checksum
 */
void PanasonicACCNT::send_command(std::vector<uint8_t> command, CommandType type, uint8_t header = CNT::CTRL_HEADER) {
  uint8_t length = command.size();
  command.insert(command.begin(), header);
  command.insert(command.begin() + 1, length);

  uint8_t checksum = 0;

  for (uint8_t i : command)
    checksum -= i;  // Add to checksum

  command.push_back(checksum);

  send_packet(command, type);  // Actually send the constructed packet
}

/*
 * Send a raw packet, as is
 */
void PanasonicACCNT::send_packet(const std::vector<uint8_t> &packet, CommandType type) {
  this->last_packet_sent_ = millis();  // Save the time when we sent the last packet

  if (type != CommandType::Response)     // Don't wait for a response for responses
    this->waiting_for_response_ = true;  // Mark that we are waiting for a response

  write_array(packet);       // Write to UART
  log_packet(packet, true);  // Write to log
}

/*
 * Loop handling
 */

void PanasonicACCNT::handle_poll() {
  if (millis() - this->last_packet_sent_ > POLL_INTERVAL) {
    ESP_LOGV(TAG, "Polling AC");
    send_command(CMD_POLL, CommandType::Normal, POLL_HEADER);
  }
}

/*
 * Packet handling
 */

bool PanasonicACCNT::verify_packet() {
  if (this->rx_buffer_.size() < 12) {
    ESP_LOGW(TAG, "Dropping invalid packet (length)");

    this->rx_buffer_.clear();  // Reset buffer
    return false;
  }

  // Check if header matches
  if (this->rx_buffer_[0] != CTRL_HEADER && this->rx_buffer_[0] != POLL_HEADER) {
    ESP_LOGW(TAG, "Dropping invalid packet (header)");

    this->rx_buffer_.clear();  // Reset buffer
    return false;
  }

  // Packet length minus header, packet length and checksum
  if (this->rx_buffer_[1] != this->rx_buffer_.size() - 3) {
    ESP_LOGD(TAG, "Dropping invalid packet (length mismatch)");

    this->rx_buffer_.clear();  // Reset buffer
    return false;
  }

  uint8_t checksum = 0;

  for (uint8_t b : this->rx_buffer_) {
    checksum += b;
  }

  if (checksum != 0) {
    ESP_LOGD(TAG, "Dropping invalid packet (checksum)");

    this->rx_buffer_.clear();  // Reset buffer
    return false;
  }

  return true;
}

void PanasonicACCNT::handle_packet() {
  if (this->rx_buffer_[0] == POLL_HEADER) {
    this->data = std::vector<uint8_t>(this->rx_buffer_.begin() + 2, this->rx_buffer_.begin() + 12);

    this->set_data(true);
    this->publish_state();

    if (this->state_ != ACState::Ready)
      this->state_ = ACState::Ready;  // Mark as ready after first poll
  } else {
    ESP_LOGD(TAG, "Received unknown packet");
  }
}

climate::ClimateMode PanasonicACCNT::determine_mode(uint8_t mode) {
  uint8_t nib1 = (mode >> 4) & 0x0F;  // Left nib for mode
  uint8_t nib2 = (mode >> 0) & 0x0F;  // Right nib for power state

  if (nib2 == 0x00)
    return climate::CLIMATE_MODE_OFF;

  switch (nib1) {
    case 0x00:  // Auto
      return climate::CLIMATE_MODE_HEAT_COOL;
    case 0x03:  // Cool
      return climate::CLIMATE_MODE_COOL;
    case 0x04:  // Heat
      return climate::CLIMATE_MODE_HEAT;
    case 0x02:  // Dry
      return climate::CLIMATE_MODE_DRY;
    case 0x06:  // Fan only
      return climate::CLIMATE_MODE_FAN_ONLY;
    default:
      ESP_LOGW(TAG, "Received unknown climate mode");
      return climate::CLIMATE_MODE_OFF;
  }
}

std::string PanasonicACCNT::determine_fan_speed(uint8_t speed) {
  switch (speed) {
    case 0xA0:  // Auto
      return "Automatic";
    case 0x30:  // 1
      return "1";
    case 0x40:  // 2
      return "2";
    case 0x50:  // 3
      return "3";
    case 0x60:  // 4
      return "4";
    case 0x70:  // 5
      return "5";
    default:
      ESP_LOGW(TAG, "Received unknown fan speed");
      return "Unknown";
  }
}

std::string PanasonicACCNT::determine_vertical_swing(uint8_t swing) {
  uint8_t nib = (swing >> 4) & 0x0F;  // Left nib for vertical swing

  switch (nib) {
    case 0x0F:
      return "auto";
    case 0x01:
      return "up";
    case 0x02:
      return "up_center";
    case 0x03:
      return "center";
    case 0x04:
      return "down_center";
    case 0x05:
      return "down";
    case 0x00:
      return "unsupported";
    default:
      ESP_LOGW(TAG, "Received unknown vertical swing mode");
      return "Unknown";
  }
}

std::string PanasonicACCNT::determine_horizontal_swing(uint8_t swing) {
  uint8_t nib = (swing >> 0) & 0x0F;  // Right nib for horizontal swing

  switch (nib) {
    case 0x0D:
      return "auto";
    case 0x09:
      return "left";
    case 0x0A:
      return "left_center";
    case 0x06:
      return "center";
    case 0x0B:
      return "right_center";
    case 0x0C:
      return "right";
    case 0x00:
      return "unsupported";
    default:
      ESP_LOGW(TAG, "Received unknown horizontal swing mode");
      return "Unknown";
  }
}

std::string PanasonicACCNT::determine_preset(uint8_t preset) {
  uint8_t nib = (preset >> 0) & 0x0F;  // Right nib for preset (powerful/quiet)

  switch (nib) {
    case 0x02:
      // return climate::CLIMATE_PRESET_BOOST;
      return "Powerful";
    case 0x04:
      // return climate::CLIMATE_PRESET_ECO;
      return "Quiet";
    case 0x00:
      // return climate::CLIMATE_PRESET_NONE;
      return "Normal";
    default:
      ESP_LOGW(TAG, "Received unknown preset");
      // return climate::CLIMATE_PRESET_NONE;
      return "Normal";
  }
}

bool PanasonicACCNT::determine_preset_nanoex(uint8_t preset) {
  uint8_t nib = (preset >> 4) & 0x0F;  // Left nib for nanoex

  if (nib == 0x04)
    return true;
  else if (nib == 0x00)
    return false;
  else {
    ESP_LOGW(TAG, "Received unknown nanoex value");
    return false;
  }
}

bool PanasonicACCNT::determine_eco(uint8_t value) {
  if (value == 0x40)
    return true;
  else if (value == 0x00)
    return false;
  else {
    ESP_LOGW(TAG, "Received unknown eco value");
    return false;
  }
}

bool PanasonicACCNT::determine_mild_dry(uint8_t value) {
  if (value == 0x7F)
    return true;
  else if (value == 0x80)
    return false;
  else {
    ESP_LOGW(TAG, "Received unknown mild dry value");
    return false;
  }
}

/*
 * Sensor handling
 */

void PanasonicACCNT::on_vertical_swing_change(const std::string &swing) {
  if (this->state_ != ACState::Ready)
    return;

  ESP_LOGD(TAG, "Setting vertical swing position");

  if (swing == "down")
    this->data[4] = (this->data[4] & 0x0F) + 0x50;
  else if (swing == "down_center")
    this->data[4] = (this->data[4] & 0x0F) + 0x40;
  else if (swing == "center")
    this->data[4] = (this->data[4] & 0x0F) + 0x30;
  else if (swing == "up_center")
    this->data[4] = (this->data[4] & 0x0F) + 0x20;
  else if (swing == "up")
    this->data[4] = (this->data[4] & 0x0F) + 0x10;
  else if (swing == "auto")
    this->data[4] = (this->data[4] & 0x0F) + 0xF0;
  else {
    ESP_LOGW(TAG, "Unsupported vertical swing position received");
    return;
  }

  send_command(this->data, CommandType::Normal, CTRL_HEADER);
}

void PanasonicACCNT::on_horizontal_swing_change(const std::string &swing) {
  if (this->state_ != ACState::Ready)
    return;

  ESP_LOGD(TAG, "Setting horizontal swing position");

  if (swing == "left")
    this->data[4] = (this->data[4] & 0xF0) + 0x09;
  else if (swing == "left_center")
    this->data[4] = (this->data[4] & 0xF0) + 0x0A;
  else if (swing == "center")
    this->data[4] = (this->data[4] & 0xF0) + 0x06;
  else if (swing == "right_center")
    this->data[4] = (this->data[4] & 0xF0) + 0x0B;
  else if (swing == "right")
    this->data[4] = (this->data[4] & 0xF0) + 0x0C;
  else if (swing == "auto")
    this->data[4] = (this->data[4] & 0xF0) + 0x0D;
  else {
    ESP_LOGW(TAG, "Unsupported horizontal swing position received");
    return;
  }

  send_command(this->data, CommandType::Normal, CTRL_HEADER);
}

void PanasonicACCNT::on_nanoex_change(bool state) {
  if (this->state_ != ACState::Ready)
    return;

  this->nanoex_state_ = state;

  if (state) {
    ESP_LOGV(TAG, "Turning nanoex on");
    this->data[5] = (this->data[5] & 0x0F) + 0x40;
  } else {
    ESP_LOGV(TAG, "Turning nanoex off");
    this->data[5] = (this->data[5] & 0x0F);
  }

  send_command(this->data, CommandType::Normal, CTRL_HEADER);
}

void PanasonicACCNT::on_eco_change(bool state) {
  if (this->state_ != ACState::Ready)
    return;

  this->eco_state_ = state;

  if (state) {
    ESP_LOGV(TAG, "Turning eco mode on");
    this->data[8] = 0x40;
  } else {
    ESP_LOGV(TAG, "Turning eco mode off");
    this->data[8] = 0x00;
  }

  send_command(this->data, CommandType::Normal, CTRL_HEADER);
}

void PanasonicACCNT::on_mild_dry_change(bool state) {
  if (this->state_ != ACState::Ready)
    return;

  this->mild_dry_state_ = state;

  if (state) {
    ESP_LOGV(TAG, "Turning mild dry on");
    this->data[2] = 0x7F;
  } else {
    ESP_LOGV(TAG, "Turning mild dry off");
    this->data[2] = 0x80;
  }

  send_command(this->data, CommandType::Normal, CTRL_HEADER);
}

}  // namespace CNT
}  // namespace panasonic_ac
}  // namespace esphome
