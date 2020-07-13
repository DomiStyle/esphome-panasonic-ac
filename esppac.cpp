#include "esppac.h"
#include "esppac_commands.h"

using namespace esphome;

PanasonicAC::PanasonicAC(uart::UARTComponent *parent) : uart::UARTDevice(parent) {}

climate::ClimateTraits PanasonicAC::traits()
{
  auto traits = climate::ClimateTraits();

  traits.set_supports_action(true);
  traits.set_supports_current_temperature(true);
  traits.set_supports_auto_mode(true);
  traits.set_supports_cool_mode(true);
  traits.set_supports_heat_mode(true);
  traits.set_supports_dry_mode(true);
  traits.set_supports_fan_only_mode(true);
  traits.set_supports_two_point_target_temperature(false);
  traits.set_supports_away(false);
  traits.set_visual_min_temperature(ESPPAC_MIN_TEMPERATURE);
  traits.set_visual_max_temperature(ESPPAC_MAX_TEMPERATURE);
  traits.set_visual_temperature_step(ESPPAC_TEMPERATURE_STEP);
  traits.set_supports_fan_mode_on(false);
  traits.set_supports_fan_mode_off(false);
  traits.set_supports_fan_mode_auto(true);
  traits.set_supports_fan_mode_focus(true);
  traits.set_supports_fan_mode_diffuse(true);
  traits.set_supports_fan_mode_low(true);
  traits.set_supports_fan_mode_medium(true);
  traits.set_supports_fan_mode_middle(true);
  traits.set_supports_fan_mode_high(true);
  traits.set_supports_swing_mode_off(true);
  traits.set_supports_swing_mode_both(true);
  traits.set_supports_swing_mode_vertical(true);
  traits.set_supports_swing_mode_horizontal(true);

  return traits;
}

void PanasonicAC::setup()
{
  // Initialize times
  initTime = millis();
  lastPacketSent = millis();

  this->fan_mode = climate::CLIMATE_FAN_OFF;
  this->swing_mode = climate::CLIMATE_SWING_OFF;
  this->publish_state(); // Post dummy state so Home Assistant doesn't disconnect

  ESP_LOGI(TAG, "Panasonic AC component v%s", ESPPAC_VERSION);
}

void PanasonicAC::loop()
{
  if(state != ACState::Ready)
  {
    handle_init_packets(); // Handle initialization packets separate from normal packets

    if(millis() - initTime > ESPPAC_INIT_FAIL_TIMEOUT)
    {
      state = ACState::Failed;
      mark_failed();
      return;
    }
  }

  if(millis() - lastRead > ESPPAC_READ_TIMEOUT && receiveBufferIndex != 0) // Check if our read timed out and we received something
  {
    log_packet(receiveBuffer, receiveBufferIndex);

    if(!verify_packet()) // Verify length, header, counter and checksum
      return;

    waitingForResponse = false; // Set that we are not waiting for a response anymore since we received a valid one
    lastPacketReceived = millis(); // Set the time at which we received our last packet

    if(state == ACState::Ready || state == ACState::FirstPoll || state == ACState::HandshakeEnding) // Parse regular packets
    {
      handle_packet(); // Handle regular packet
    }
    else // Parse handshake packets
    {
      handle_handshake_packet(); // Not initialized yet, handle handshake packet
    }

    receiveBufferIndex = 0; // Reset buffer
  }

  read_data(); // Read data from UART (if there is any)

  handle_resend(); // Handle packets that need to be resent

  handle_poll(); // Handle sending poll packets
}

/*
 * ESPHome control request
 */

void PanasonicAC::control(const climate::ClimateCall &call)
{
  if(this->state != ACState::Ready)
    return;

  ESP_LOGV(TAG, "AC control request");

  if(call.get_mode().has_value())
  {
    ESP_LOGV(TAG, "Requested mode change");

    switch(*call.get_mode())
    {
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
      case climate::CLIMATE_MODE_AUTO:
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

    this->mode = *call.get_mode(); // Set mode manually since we won't receive a report from the AC if its the same mode again
    this->publish_state(); // Send this state, will get updated once next poll is executed
  }

  if(call.get_target_temperature().has_value())
  {
    ESP_LOGV(TAG, "Requested temperature change");
    set_value(0x31, *call.get_target_temperature() * 2);
  }

  if(call.get_fan_mode().has_value())
  {
    ESP_LOGV(TAG, "Requested fan mode change");

    switch(*call.get_fan_mode())
    {
      case climate::CLIMATE_FAN_LOW:
        set_value(0xB2, 0x41);
        set_value(0xA0, 0x32);
      break;
      case climate::CLIMATE_FAN_MEDIUM:
        set_value(0xB2, 0x41);
        set_value(0xA0, 0x33);
      break;
      case climate::CLIMATE_FAN_MIDDLE:
        set_value(0xB2, 0x41);
        set_value(0xA0, 0x34);
      break;
      case climate::CLIMATE_FAN_HIGH:
        set_value(0xB2, 0x41);
        set_value(0xA0, 0x36);
      break;
      case climate::CLIMATE_FAN_AUTO:
        set_value(0xB2, 0x41);
        set_value(0xA0, 0x41);
      break;
      case climate::CLIMATE_FAN_DIFFUSE:
        set_value(0xB2, 0x43);
        set_value(0x35, 0x42);
        set_value(0x34, 0x42);
      break;
      case climate::CLIMATE_FAN_FOCUS:
        set_value(0xB2, 0x42);
        set_value(0x35, 0x42);
        set_value(0x34, 0x42);
      break;
      default:
        ESP_LOGV(TAG, "Unsupported fan mode requested");
      break;
    }
  }

  if(call.get_swing_mode().has_value())
  {
    ESP_LOGV(TAG, "Requested swing mode change");

    switch(*call.get_swing_mode())
    {
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

  if(setQueueIndex > 0) // Only send packet if any changes need to be made
  {
    send_set_command();
  }
}

/*
 * Loop handling
 */

void PanasonicAC::handle_poll()
{
  if(state == ACState::Ready && millis() - lastPacketSent > ESPPAC_POLL_INTERVAL)
  {
    ESP_LOGV(TAG, "Polling AC");
    send_command(CMD_POLL, sizeof(CMD_POLL));
  }
}

void PanasonicAC::handle_init_packets()
{
  if(state == ACState::Initializing)
  {
    if(millis() - initTime > ESPPAC_INIT_TIMEOUT) // Handle handshake initialization
    {
      ESP_LOGD(TAG, "Starting handshake [1/16]");
      send_command(CMD_HANDSHAKE_1, sizeof(CMD_HANDSHAKE_1)); // Send first handshake packet, AC won't send a response
      delay(3); // Add small delay to mimic real wifi adapter
      send_command(CMD_HANDSHAKE_2, sizeof(CMD_HANDSHAKE_2)); // Send second handshake packet, AC won't send a response but we will trigger a resend

      state = ACState::Handshake; // Update state to handshake started
    }
  }
  else if(state == ACState::FirstPoll && millis() - lastPacketSent > ESPPAC_FIRST_POLL_TIMEOUT) // Handle sending first poll
  {
    ESP_LOGD(TAG, "Polling for the first time");
    send_command(CMD_POLL, sizeof(CMD_POLL));

    state = ACState::HandshakeEnding;
  }
  else if(state == ACState::HandshakeEnding && millis() - lastPacketSent > ESPPAC_INIT_END_TIMEOUT) // Handle last handshake message
  {
    ESP_LOGD(TAG, "Finishing handshake [16/16]");
    send_command(CMD_HANDSHAKE_16, sizeof(CMD_HANDSHAKE_16));

    // State is set to ready in the response to this packet
  }
}

bool PanasonicAC::verify_packet()
{
  if(receiveBufferIndex < 5) // Drop packets that are too short
  {
    ESP_LOGW(TAG, "Dropping invalid packet (length)");
    receiveBufferIndex = 0; // Reset buffer
    return false;
  }

  if(receiveBuffer[0] == 0x66) // Sync packets are the only packet not starting with 0x5A
  {
    ESP_LOGI(TAG, "Received sync packet, triggering initialization");
    initTime -= ESPPAC_INIT_TIMEOUT; // Set init time back to trigger a initialization now
    receiveBufferIndex = 0; // Reset buffer
    return false;
  }

  if(receiveBuffer[0] != ESPPAC_HEADER) // Check if header matches
  {
    ESP_LOGW(TAG, "Dropping invalid packet (header)");
    receiveBufferIndex = 0; // Reset buffer
    return false;
  }

  if(state == ACState::Ready && waitingForResponse) // If we were waiting for a response, check if the tx packet counter matches (if we are ready)
  {
    if(receiveBuffer[1] != transmitPacketCount - 1 && receiveBuffer[1] != 0xFE) // Check transmit packet counter
    {
      ESP_LOGW(TAG, "Correcting shifted tx counter");
      receivePacketCount = receiveBuffer[1];
    }
  }
  else if(state == ACState::Ready) // If we were not waiting for a response, check if the rx packet counter matches (if we are ready)
  {
    if(receiveBuffer[1] != receivePacketCount) // Check receive packet counter
    {
      ESP_LOGW(TAG, "Correcting shifted rx counter");
      receivePacketCount = receiveBuffer[1];
    }
  }

  byte checksum = receiveBuffer[0]; // Set checksum to first byte

  for(int i = 1; i < receiveBufferIndex; i++) // Add all bytes together
  {
    checksum += receiveBuffer[i];
  }

  if(checksum != 0) // Check if checksum is valid
  {
    ESP_LOGD(TAG, "Dropping invalid packet (checksum)");

    receiveBufferIndex = 0; // Reset buffer
    return false;
  }

  return true;
}

/*
 * Field handling
 */

void PanasonicAC::determine_mode(byte mode)
{
  switch(mode) // Check mode
  {
    case 0x41: // Auto
      this->mode = climate::CLIMATE_MODE_AUTO;
    break;
    case 0x42: // Cool
      this->mode = climate::CLIMATE_MODE_COOL;
    break;
    case 0x43: // Heat
      this->mode = climate::CLIMATE_MODE_HEAT;
    break;
    case 0x44: // Dry
      this->mode = climate::CLIMATE_MODE_DRY;
    break;
    case 0x45: // Fan only?
      this->mode = climate::CLIMATE_MODE_FAN_ONLY;
    break;
    default:
      ESP_LOGW(TAG, "Received unknown climate mode");
    break;
  }
}

void PanasonicAC::determine_fan_speed(byte speed)
{
  switch(speed)
  {
    case 0x32: // 1
      this->fan_mode = climate::CLIMATE_FAN_LOW;
    break;
    case 0x33: // 2
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
    break;
    case 0x34: // 3
      this->fan_mode = climate::CLIMATE_FAN_MIDDLE;
    break;
    case 0x35: // 4
    case 0x36: // 5
      this->fan_mode = climate::CLIMATE_FAN_HIGH; // Take 4 & 5 together for high since ESPHome doesn't have that many
    break;
    case 0x41: // Auto
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
    break;
    default:
      ESP_LOGW(TAG, "Received unknown fan speed");
    break;
  }
}

void PanasonicAC::determine_fan_power(byte power)
{
  switch(power)
  {
    case 0x43: // Quiet
      this->fan_mode = climate::CLIMATE_FAN_DIFFUSE;
    break;
    case 0x42: // Powerful
      this->fan_mode = climate::CLIMATE_FAN_FOCUS;
    break;
    case 0x41: // Normal (see determine_fan_speed)
      // Ignore
    break;
    default:
      ESP_LOGW(TAG, "Received unknown fan power");
    break;
  }
}

void PanasonicAC::determine_swing(byte swing)
{
  switch(swing)
  {
    case 0x41: // Both
      this->swing_mode = climate::CLIMATE_SWING_BOTH;
    break;
    case 0x42: // Off
      this->swing_mode = climate::CLIMATE_SWING_OFF;
    break;
    case 0x43: // Vertical
      this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
    break;
    case 0x44: // Horizontal
      this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
    break;
    default:
      ESP_LOGW(TAG, "Received unknown swing mode");
    break;
  }
}

void PanasonicAC::determine_action()
{
  if(this->mode == climate::CLIMATE_MODE_OFF)
  {
    this->action = climate::CLIMATE_ACTION_OFF;
  }
  else if(this->mode == climate::CLIMATE_MODE_FAN_ONLY)
  {
    this->action = climate::CLIMATE_ACTION_FAN;
  }
  else if(this->mode == climate::CLIMATE_MODE_DRY)
  {
    this->action = climate::CLIMATE_ACTION_DRYING;
  }
  else if((this->mode == climate::CLIMATE_MODE_COOL || this->mode == climate::CLIMATE_MODE_AUTO) && this->current_temperature + ESPPAC_TEMPERATURE_TOLERANCE >= this->target_temperature)
  {
    this->action = climate::CLIMATE_ACTION_COOLING;
  }
  else if((this->mode == climate::CLIMATE_MODE_HEAT || this->mode == climate::CLIMATE_MODE_AUTO) && this->current_temperature - ESPPAC_TEMPERATURE_TOLERANCE <= this->target_temperature)
  {
    this->action = climate::CLIMATE_ACTION_HEATING;
  }
  else
  {
    this->action = climate::CLIMATE_ACTION_IDLE;
  }
}

const char* PanasonicAC::get_swing_vertical(byte swing)
{
  switch(swing)
  {
    case 0x42: // Down
      return "down";
    break;
    case 0x45: // Down center
      return "down_center";
    break;
    case 0x43: // Center
      return "center";
    break;
    case 0x44: // Up Center
      return "up_center";
    break;
    case 0x41: // Up
      return "up";
    break;
    default:
      ESP_LOGW(TAG, "Received unknown vertical swing position");
      return "unknown";
    break;
  }
}

const char* PanasonicAC::get_swing_horizontal(byte swing)
{
  switch(swing)
  {
    case 0x42: // Left
      return "left";
    break;
    case 0x5C: // Left center
      return "left_center";
    break;
    case 0x43: // Center
      return "center";
    break;
    case 0x56: // Right center
      return "right_center";
    break;
    case 0x41: // Right
      return "right";
    break;
    default:
      ESP_LOGW(TAG, "Received unknown horizontal swing position");
      return "unknown";
    break;
  }
}

/*
 * Packet handling
 */

void PanasonicAC::handle_packet()
{
  if(receiveBuffer[2] == 0x01 && receiveBuffer[3] == 0x01) // Ping
  {
    ESP_LOGD(TAG, "Answering ping");
    send_command(CMD_PING, sizeof(CMD_PING), CommandType::Response);
  }
  else if(receiveBuffer[2] == 0x10 && receiveBuffer[3] == 0x89) // Received query response
  {
    ESP_LOGD(TAG, "Received query response");

    if(receiveBufferIndex != 125)
    {
      ESP_LOGW(TAG, "Received invalid query response");
      return;
    }

    if(receiveBuffer[14] == 0x31) // Check if power state is off
      this->mode = climate::CLIMATE_MODE_OFF; // Climate is off
    else
    {
      determine_mode(receiveBuffer[18]); // Check mode if power state is not off
    }

    this->target_temperature = receiveBuffer[22] * ESPPAC_TEMPERATURE_STEP; // Set temperature
    this->current_temperature = receiveBuffer[62]; // Set current (inside) temperature; no temperature steps

    update_outside_temperature(receiveBuffer[66]);

    update_swing_horizontal(receiveBuffer[34]);

    update_swing_vertical(receiveBuffer[38]);

    update_nanoex(receiveBuffer[50]);

    determine_fan_speed(receiveBuffer[26]);
    determine_fan_power(receiveBuffer[42]); // Fan power can overwrite fan speed

    determine_swing(receiveBuffer[30]);

    determine_action(); // Determine the current action of the AC

    this->publish_state();
  }
  else if(receiveBuffer[2] == 0x10 && receiveBuffer[3] == 0x88) // Command ack
  {
    ESP_LOGV(TAG, "Received command ack");
  }
  else if(receiveBuffer[2] == 0x10 && receiveBuffer[3] == 0x0A) // Report
  {
    ESP_LOGV(TAG, "Received report");
    send_command(CMD_REPORT_ACK, sizeof(CMD_REPORT_ACK), CommandType::Response);

    if(receiveBufferIndex < 13)
    {
      ESP_LOGE(TAG, "Report is too short to handle");
      return;
    }

    // 0 = Header & packet type
    // 1 = Packet length
    // 2 = Key value pair counter
    for(int i = 0; i < receiveBuffer[10]; i++)
    {
      // Offset everything by header, packet length and pair counter (4 * 3)
      // then offset by pair length (i * 4)
      int currentIndex = (4 * 3) + (i * 4);

      // 0 = Header
      // 1 = Data
      // 2 = Data
      // 3 = ?
      switch(receiveBuffer[currentIndex])
      {
        case 0x80: // Power mode
          switch(receiveBuffer[currentIndex + 2])
          {
            case 0x30: // Power mode on
              ESP_LOGV(TAG, "Received power mode on");
              // Ignore power on and let mode be set by other report
            break;
            case 0x31: // Power mode off
              ESP_LOGV(TAG, "Received power mode off");
              this->mode = climate::CLIMATE_MODE_OFF;
            break;
            default:
              ESP_LOGW(TAG, "Received unknown power mode");
            break;
          }
        break;
        case 0xB0: // Mode
          determine_mode(receiveBuffer[currentIndex + 2]);
        break;
        case 0x31: // Target temperature
          ESP_LOGV(TAG, "Received target temperature");
          this->target_temperature = receiveBuffer[currentIndex + 2] * ESPPAC_TEMPERATURE_STEP;
        break;
        case 0xA0: // Fan speed
          ESP_LOGV(TAG, "Received fan speed");
          determine_fan_speed(receiveBuffer[currentIndex + 2]);
        case 0xB2:
          ESP_LOGV(TAG, "Received fan power mode");
          determine_fan_power(receiveBuffer[currentIndex + 2]);
        break;
        case 0xA1:
          ESP_LOGV(TAG, "Received swing mode");
          determine_swing(receiveBuffer[currentIndex + 2]);
        break;
        case 0xA5: // Horizontal swing position
          ESP_LOGV(TAG, "Received horizontal swing position");

          update_swing_horizontal(receiveBuffer[currentIndex + 2]);
        break;
        case 0xA4: // Vertical swing position
          ESP_LOGV(TAG, "Received vertical swing position");

          update_swing_vertical(receiveBuffer[currentIndex + 2]);
        break;
        case 0x33: // nanoex mode
          ESP_LOGV(TAG, "Received nanoex state");

          update_nanoex(receiveBuffer[currentIndex + 2]);
        break;
        case 0x20:
          ESP_LOGV(TAG, "Received unknown nanoex field");
          // Not sure what this one, ignore it for now
        break;
        default:
          ESP_LOGW(TAG, "Report has unknown field");
        break;
      }
    }

    determine_action(); // Determine the current action of the AC

    this->publish_state();
  }
  else if(receiveBuffer[2] == 0x01 && receiveBuffer[3] == 0x80) // Answer for handshake 16
  {
    ESP_LOGI(TAG, "Handshake completed");
    state = ACState::Ready;
  }
  else
  {
    ESP_LOGW(TAG, "Received unknown packet");
  }
}

void PanasonicAC::handle_handshake_packet()
{
  if(receiveBuffer[2] == 0x00 && receiveBuffer[3] == 0x89) // Answer for handshake 2
  {
    ESP_LOGD(TAG, "Answering handshake [2/16]");
    send_command(CMD_HANDSHAKE_3, sizeof(CMD_HANDSHAKE_3));
  }
  else if(receiveBuffer[2] == 0x00 && receiveBuffer[3] == 0x8C) // Answer for handshake 3
  {
    ESP_LOGD(TAG, "Answering handshake [3/16]");
    send_command(CMD_HANDSHAKE_4, sizeof(CMD_HANDSHAKE_4));
  }
  else if(receiveBuffer[2] == 0x00 && receiveBuffer[3] == 0x90) // Answer for handshake 4
  {
    ESP_LOGD(TAG, "Answering handshake [4/16]");
    send_command(CMD_HANDSHAKE_5, sizeof(CMD_HANDSHAKE_5));
  }
  else if(receiveBuffer[2] == 0x00 && receiveBuffer[3] == 0x91) // Answer for handshake 5
  {
    ESP_LOGD(TAG, "Answering handshake [5/16]");
    send_command(CMD_HANDSHAKE_6, sizeof(CMD_HANDSHAKE_6));
  }
  else if(receiveBuffer[2] == 0x00 && receiveBuffer[3] == 0x92) // Answer for handshake 6
  {
    ESP_LOGD(TAG, "Answering handshake [6/16]");
    send_command(CMD_HANDSHAKE_7, sizeof(CMD_HANDSHAKE_7));
  }
  else if(receiveBuffer[2] == 0x00 && receiveBuffer[3] == 0xC1) // Answer for handshake 7
  {
    ESP_LOGD(TAG, "Answering handshake [7/16]");
    send_command(CMD_HANDSHAKE_8, sizeof(CMD_HANDSHAKE_8));
  }
  else if(receiveBuffer[2] == 0x01 && receiveBuffer[3] == 0xCC) // Answer for handshake 8
  {
    ESP_LOGD(TAG, "Answering handshake [8/16]");
    send_command(CMD_HANDSHAKE_9, sizeof(CMD_HANDSHAKE_9));
  }
  else if(receiveBuffer[2] == 0x10 && receiveBuffer[3] == 0x80) // Answer for handshake 9
  {
    ESP_LOGD(TAG, "Answering handshake [9/16]");
    send_command(CMD_HANDSHAKE_10, sizeof(CMD_HANDSHAKE_10));
  }
  else if(receiveBuffer[2] == 0x10 && receiveBuffer[3] == 0x81) // Answer for handshake 10
  {
    ESP_LOGD(TAG, "Answering handshake [10/16]");
    send_command(CMD_HANDSHAKE_11, sizeof(CMD_HANDSHAKE_11));
  }
  else if(receiveBuffer[2] == 0x00 && receiveBuffer[3] == 0x98) // Answer for handshake 11
  {
    ESP_LOGD(TAG, "Answering handshake [11/16]");
    send_command(CMD_HANDSHAKE_12, sizeof(CMD_HANDSHAKE_12));
  }
  else if(receiveBuffer[2] == 0x01 && receiveBuffer[3] == 0x80) // Answer for handshake 12
  {
    ESP_LOGD(TAG, "Answering handshake [12/16]");
    send_command(CMD_HANDSHAKE_13, sizeof(CMD_HANDSHAKE_13));
  }
  else if(receiveBuffer[2] == 0x10 && receiveBuffer[3] == 0x88) // Answer for handshake 13
  {
    // Ignore
    ESP_LOGD(TAG, "Ignoring handshake [13/16]");
  }
  else if(receiveBuffer[2] == 0x01 && receiveBuffer[3] == 0x09) // First unsolicited packet from AC containing rx counter
  {
    ESP_LOGD(TAG, "Received rx counter [14/16]");
    receivePacketCount = receiveBuffer[1]; // Set rx packet counter
    send_command(CMD_HANDSHAKE_14, sizeof(CMD_HANDSHAKE_14), CommandType::Response);
  }
  else if(receiveBuffer[2] == 0x00 && receiveBuffer[3] == 0x20) // Second unsolicited packet from AC
  {
    ESP_LOGD(TAG, "Answering handshake [15/16]");
    state = ACState::FirstPoll; // Start delayed first poll
    send_command(CMD_HANDSHAKE_15, sizeof(CMD_HANDSHAKE_15), CommandType::Response);
  }
  else
  {
    ESP_LOGW(TAG, "Received unknown packet during initialization");
  }
}

/*
 * Packet sending
 */

void PanasonicAC::send_set_command()
{
  // Size of packet is 3 * 4 (for the header, packet size, and key value pair counter)
  // setQueueIndex * 4 for the individual key value pairs
  int packetLength = (3 * 4) + (setQueueIndex * 4);
  byte packet[packetLength];

  // Mark this packet as a set command
  packet[2] = 0x10;
  packet[3] = 0x08;

  packet[4] = 0x00; // Packet size header
  packet[5] = (4 * setQueueIndex) - 1 + 6; // Packet size, key value pairs * 4, subtract checksum (-1), add 4 for key value pair counter and 2 for rest of packet size
  packet[6] = 0x01;
  packet[7] = 0x01;

  packet[8] = 0x30; // Key value pair counter header
  packet[9] = 0x01;
  packet[10] = setQueueIndex; // Key value pair counter
  packet[11] = 0x00;


  for(int i = 0; i < setQueueIndex; i++)
  {
    packet[12 + (i * 4) + 0] = setQueue[i][0]; // Key
    packet[12 + (i * 4) + 1] = 0x01; // Unknown, always 0x01
    packet[12 + (i * 4) + 2] = setQueue[i][1]; // Value
    packet[12 + (i * 4) + 3] = 0x00; // Unknown, either 0x00 or 0x01 or 0x02; overwritten by checksum on last key value pair
  }

  send_packet(packet, packetLength, CommandType::Normal);
  setQueueIndex = 0;
}

void PanasonicAC::send_command(const byte* command, size_t commandLength, CommandType type)
{
  byte packet[commandLength + 3]; // Reserve space for upcoming packet

  for(int i = 0; i < commandLength; i++) // Loop through command
  {
      packet[i + 2] = command[i]; // Add to packet
  }

  lastCommand = command; // Store the last command we sent
  lastCommandLength = commandLength; // Store the length of the last command we sent

  send_packet(packet, commandLength + 3, type); // Actually send the constructed packet
}

void PanasonicAC::send_packet(byte* packet, size_t packetLength, CommandType type)
{
  byte checksum = ESPPAC_HEADER; // Checksum is calculated by adding all bytes together
  packet[0] = ESPPAC_HEADER; // Write header to packet

  byte packetCount = transmitPacketCount; // Set packet counter

  if(type == CommandType::Response)
    packetCount = receivePacketCount; // Set the packet counter to the rx counter
  else if(type == CommandType::Resend)
    packetCount = transmitPacketCount - 1; // Set the packet counter to the tx counter -1 (we are sending the same packet again)

  checksum += packetCount; // Add to checksum
  packet[1] = packetCount; // Write to packet

  for(int i = 2; i < packetLength - 1; i++) // Loop through payload to calculate checksum
  {
    checksum += packet[i]; // Add byte to checksum
  }

  checksum = (~checksum + 1); // Compute checksum
  packet[packetLength - 1] = checksum; // Add checksum to end of packet

  lastPacketSent = millis(); // Save the time when we sent the last packet

  if(type == CommandType::Normal) // Do not increase tx counter if this was a response or if this was a resent packet
  {
    if(transmitPacketCount == 0xFE)
      transmitPacketCount = 0x01; // Special case, roll over transmit counter after 0xFE
    else
      transmitPacketCount++; // Increase tx packet counter if this wasn't a response
  }
  else if(type == CommandType::Response)
  {
    if(receivePacketCount == 0xFE)
      receivePacketCount = 0x01; // Special case, roll over receive counter after 0xFE
    else
      receivePacketCount++; // Increase rx counter if this was a response
  }

  if(type != CommandType::Response) // Don't wait for a response for responses
    waitingForResponse = true; // Mark that we are waiting for a response

  write_array(packet, packetLength); // Write to UART
  log_packet(packet, packetLength, true); // Write to log
}

/*
 * Helpers
 */
void PanasonicAC::read_data()
{
  while(available()) // Read while data is available
  {
    if(receiveBufferIndex >= ESPPAC_BUFFER_SIZE)
    {
      ESP_LOGE(TAG, "Receive buffer overflow");
      receiveBufferIndex = 0;
    }

    receiveBuffer[receiveBufferIndex] = read(); // Store in receive buffer

    lastRead = millis(); // Update lastRead timestamp
    receiveBufferIndex++; // Increase buffer index
  }
}

void PanasonicAC::handle_resend()
{
  if(waitingForResponse && millis() - lastPacketSent > ESPPAC_RESPONSE_TIMEOUT && receiveBufferIndex == 0) // Check if AC failed to respond in time and resend packet, if nothing was received yet
  {
    ESP_LOGD(TAG, "Resending previous packet");
    send_command(lastCommand, lastCommandLength, CommandType::Resend);
  }
}

void PanasonicAC::set_value(byte key, byte value)
{
  if(setQueueIndex >= 15)
  {
    ESP_LOGE(TAG, "Set queue overflow");
    setQueueIndex = 0;
    return;
  }

  setQueue[setQueueIndex][0] = key;
  setQueue[setQueueIndex][1] = value;
  setQueueIndex++;
}

void PanasonicAC::log_packet(byte array[], size_t length, bool outgoing)
{
  char buffer[(length*3)+4];

  if(outgoing)
  {
    buffer[0] = 'T';
    buffer[1] = 'X';
  }
  else
  {
    buffer[0] = 'R';
    buffer[1] = 'X';
  }

  buffer[2] = ':';
  buffer[3] = ' ';

  for (unsigned int i = 0; i < length; i++)
  {
    byte nib1 = (array[i] >> 4) & 0x0F;
    byte nib2 = (array[i] >> 0) & 0x0F;
    buffer[i*3+4] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
    buffer[i*3+5] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
    buffer[i*3+6] = ' ';
  }

  buffer[(length*3)+4-1] = '\0';

  ESP_LOGV(TAG, buffer);
}

/*
 * Sensor handling
 */

void PanasonicAC::set_outside_temperature_sensor(sensor::Sensor *outside_temperature_sensor)
{
  this->outside_temperature_sensor = outside_temperature_sensor;
}

void PanasonicAC::set_vertical_swing_sensor(text_sensor::TextSensor *vertical_swing_sensor)
{
  this->vertical_swing_sensor = vertical_swing_sensor;

  this->vertical_swing_sensor->add_on_state_callback([this](std::string value)
  {
    if(this->state != ACState::Ready)
      return;

    if(value != this->vertical_swing_state) // Ignore if already the correct state
    {
      ESP_LOGD(TAG, "Setting vertical swing position");

      if(value == "down")
        set_value(0xA4, 0x42);
      else if(value == "down_center")
        set_value(0xA4, 0x45);
      else if(value == "center")
        set_value(0xA4, 0x43);
      else if(value == "up_center")
        set_value(0xA4, 0x44);
      else if(value == "up")
        set_value(0xA4, 0x41);

      send_set_command();
    }
  });
}

void PanasonicAC::set_horizontal_swing_sensor(text_sensor::TextSensor *horizontal_swing_sensor)
{
  this->horizontal_swing_sensor = horizontal_swing_sensor;

  this->horizontal_swing_sensor->add_on_state_callback([this](std::string value)
  {
    if(this->state != ACState::Ready)
      return;

    if(value != this->horizontal_swing_state) // Ignore if already the correct state
    {
      ESP_LOGD(TAG, "Setting horizontal swing position");

      if(value == "left")
        set_value(0xA5, 0x42);
      else if(value == "left_center")
        set_value(0xA5, 0x5C);
      else if(value == "center")
        set_value(0xA5, 0x43);
      else if(value == "right_center")
        set_value(0xA5, 0x56);
      else if(value == "right")
        set_value(0xA5, 0x41);

      send_set_command();
    }
  });
}

void PanasonicAC::set_nanoex_switch(switch_::Switch *nanoex_switch)
{
  this->nanoex_switch = nanoex_switch;

  this->nanoex_switch->add_on_state_callback([this](bool value)
  {
    if(this->state != ACState::Ready)
      return;

    if(value != this->nanoex_state) // Ignore if already the correct state
    {
      if(value)
      {
        ESP_LOGV(TAG, "Turning nanoex on");
        set_value(0x33, 0x45); // nanoeX on
      }
      else
      {
        ESP_LOGV(TAG, "Turning nanoex off");
        set_value(0x33, 0x42); // nanoeX off
      }

      send_set_command();
    }
  });
}

void PanasonicAC::update_outside_temperature(byte temperature)
{
  if(this->outside_temperature_sensor != NULL && this->outside_temperature_sensor->state != temperature)
    this->outside_temperature_sensor->publish_state(temperature); // Set current (outside) temperature; no temperature steps
}

void PanasonicAC::update_swing_horizontal(byte swing)
{
  this->horizontal_swing_state = get_swing_horizontal(swing);

  if(this->horizontal_swing_sensor != NULL && this->horizontal_swing_sensor->state != this->horizontal_swing_state)
  {
    this->horizontal_swing_sensor->publish_state(this->horizontal_swing_state); // Set current horizontal swing position
  }
}

void PanasonicAC::update_swing_vertical(byte swing)
{
  this->vertical_swing_state = get_swing_vertical(swing);

  if(this->vertical_swing_sensor != NULL && this->vertical_swing_sensor->state != this->vertical_swing_state)
    this->vertical_swing_sensor->publish_state(this->vertical_swing_state); // Set current vertical swing position
}

void PanasonicAC::update_nanoex(byte nanoex)
{
  if(this->nanoex_switch != NULL)
    {
      this->nanoex_state = nanoex != 0x42;
      this->nanoex_switch->publish_state(this->nanoex_state);
    }
}
