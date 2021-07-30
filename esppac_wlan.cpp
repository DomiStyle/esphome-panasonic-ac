#include "esppac_wlan.h"
#include "esppac_commands_wlan.h"

namespace ESPPAC
{
  namespace WLAN
  {
    PanasonicACWLAN::PanasonicACWLAN(uart::UARTComponent *uartComponent) : ESPPAC::PanasonicAC(uartComponent) {}

    void PanasonicACWLAN::setup()
    {
      PanasonicAC::setup();

      ESP_LOGD(ESPPAC::TAG, "Using DNSK-P11 protocol via CN-WLAN");
    }

    void PanasonicACWLAN::loop()
    {
      if(state != ACState::Ready)
      {
        handle_init_packets(); // Handle initialization packets separate from normal packets

        if(millis() - initTime > ESPPAC::WLAN::INIT_FAIL_TIMEOUT)
        {
          state = ACState::Failed;
          mark_failed();
          return;
        }
      }

      if(millis() - lastRead > ESPPAC::READ_TIMEOUT && receiveBufferIndex != 0) // Check if our read timed out and we received something
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

      PanasonicAC::read_data();

      handle_resend(); // Handle packets that need to be resent

      handle_poll(); // Handle sending poll packets
    }

    /*
    * ESPHome control request
    */

    void PanasonicACWLAN::control(const climate::ClimateCall &call)
    {
      if(this->state != ACState::Ready)
        return;

      PanasonicAC::control(call);

      if(call.get_mode().has_value())
      {
        ESP_LOGV(ESPPAC::TAG, "Requested mode change");

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
            ESP_LOGV(ESPPAC::TAG, "Unsupported mode requested");
          break;
        }

        this->mode = *call.get_mode(); // Set mode manually since we won't receive a report from the AC if its the same mode again
        this->publish_state(); // Send this state, will get updated once next poll is executed
      }

      if(call.get_target_temperature().has_value())
      {
        ESP_LOGV(ESPPAC::TAG, "Requested temperature change");
        set_value(0x31, *call.get_target_temperature() * 2);
      }

      if(call.get_custom_fan_mode().has_value())
      {
        ESP_LOGV(ESPPAC::TAG, "Requested fan mode change");

        std::string fanMode = *call.get_custom_fan_mode();

        if(fanMode.compare("Automatic") == 0)
        {
          set_value(0xB2, 0x41);
          set_value(0xA0, 0x41);
        }
        else if(fanMode.compare("1") == 0)
        {
          set_value(0xB2, 0x41);
          set_value(0xA0, 0x32);
        }
        else if(fanMode.compare("2") == 0)
        {
          set_value(0xB2, 0x41);
          set_value(0xA0, 0x33);
        }
        else if(fanMode.compare("3") == 0)
        {
          set_value(0xB2, 0x41);
          set_value(0xA0, 0x34);
        }
        else if(fanMode.compare("4") == 0)
        {
          set_value(0xB2, 0x41);
          set_value(0xA0, 0x35);
        }
        else if(fanMode.compare("5") == 0)
        {
          set_value(0xB2, 0x41);
          set_value(0xA0, 0x36);
        }
        else
          ESP_LOGV(ESPPAC::TAG, "Unsupported fan mode requested");
      }

      if(call.get_swing_mode().has_value())
      {
        ESP_LOGV(ESPPAC::TAG, "Requested swing mode change");

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
            ESP_LOGV(ESPPAC::TAG, "Unsupported swing mode requested");
          break;
        }
      }

      if(call.get_custom_preset().has_value())
      {
        ESP_LOGV(ESPPAC::TAG, "Requested preset change");

        std::string preset = *call.get_custom_preset();

        if(preset.compare("Normal") == 0)
        {
          set_value(0xB2, 0x41);
          set_value(0x35, 0x42);
          set_value(0x34, 0x42);
        }
        else if(preset.compare("Powerful") == 0)
        {
          set_value(0xB2, 0x42);
          set_value(0x35, 0x42);
          set_value(0x34, 0x42);
        }
        else if(preset.compare("Quiet") == 0)
        {
          set_value(0xB2, 0x43);
          set_value(0x35, 0x42);
          set_value(0x34, 0x42);
        }
        else
          ESP_LOGV(ESPPAC::TAG, "Unsupported preset requested");
      }

      if(setQueueIndex > 0) // Only send packet if any changes need to be made
      {
        send_set_command();
      }
    }

    /*
    * Loop handling
    */

    void PanasonicACWLAN::handle_poll()
    {
      if(state == ACState::Ready && millis() - lastPacketSent > ESPPAC::WLAN::POLL_INTERVAL)
      {
        ESP_LOGV(ESPPAC::TAG, "Polling AC");
        send_command(ESPPAC::WLAN::CMD_POLL, sizeof(ESPPAC::WLAN::CMD_POLL));
      }
    }

    void PanasonicACWLAN::handle_init_packets()
    {
      if(state == ACState::Initializing)
      {
        if(millis() - initTime > ESPPAC::WLAN::INIT_TIMEOUT) // Handle handshake initialization
        {
          ESP_LOGD(ESPPAC::TAG, "Starting handshake [1/16]");
          send_command(ESPPAC::WLAN::CMD_HANDSHAKE_1, sizeof(ESPPAC::WLAN::CMD_HANDSHAKE_1)); // Send first handshake packet, AC won't send a response
          delay(3); // Add small delay to mimic real wifi adapter
          send_command(ESPPAC::WLAN::CMD_HANDSHAKE_2, sizeof(ESPPAC::WLAN::CMD_HANDSHAKE_2)); // Send second handshake packet, AC won't send a response but we will trigger a resend

          state = ACState::Handshake; // Update state to handshake started
        }
      }
      else if(state == ACState::FirstPoll && millis() - lastPacketSent > ESPPAC::WLAN::FIRST_POLL_TIMEOUT) // Handle sending first poll
      {
        ESP_LOGD(ESPPAC::TAG, "Polling for the first time");
        send_command(ESPPAC::WLAN::CMD_POLL, sizeof(ESPPAC::WLAN::CMD_POLL));

        state = ACState::HandshakeEnding;
      }
      else if(state == ACState::HandshakeEnding && millis() - lastPacketSent > ESPPAC::WLAN::INIT_END_TIMEOUT) // Handle last handshake message
      {
        ESP_LOGD(ESPPAC::TAG, "Finishing handshake [16/16]");
        send_command(ESPPAC::WLAN::CMD_HANDSHAKE_16, sizeof(ESPPAC::WLAN::CMD_HANDSHAKE_16));

        // State is set to ready in the response to this packet
      }
    }

    bool PanasonicACWLAN::verify_packet()
    {
      if(receiveBufferIndex < 5) // Drop packets that are too short
      {
        ESP_LOGW(ESPPAC::TAG, "Dropping invalid packet (length)");
        receiveBufferIndex = 0; // Reset buffer
        return false;
      }

      if(receiveBuffer[0] == 0x66) // Sync packets are the only packet not starting with 0x5A
      {
        ESP_LOGI(ESPPAC::TAG, "Received sync packet, triggering initialization");
        initTime -= ESPPAC::WLAN::INIT_TIMEOUT; // Set init time back to trigger a initialization now
        receiveBufferIndex = 0; // Reset buffer
        return false;
      }

      if(receiveBuffer[0] != ESPPAC::WLAN::HEADER) // Check if header matches
      {
        ESP_LOGW(ESPPAC::TAG, "Dropping invalid packet (header)");
        receiveBufferIndex = 0; // Reset buffer
        return false;
      }

      if(state == ACState::Ready && waitingForResponse) // If we were waiting for a response, check if the tx packet counter matches (if we are ready)
      {
        if(receiveBuffer[1] != transmitPacketCount - 1 && receiveBuffer[1] != 0xFE) // Check transmit packet counter
        {
          ESP_LOGW(ESPPAC::TAG, "Correcting shifted tx counter");
          receivePacketCount = receiveBuffer[1];
        }
      }
      else if(state == ACState::Ready) // If we were not waiting for a response, check if the rx packet counter matches (if we are ready)
      {
        if(receiveBuffer[1] != receivePacketCount) // Check receive packet counter
        {
          ESP_LOGW(ESPPAC::TAG, "Correcting shifted rx counter");
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
        ESP_LOGD(ESPPAC::TAG, "Dropping invalid packet (checksum)");

        receiveBufferIndex = 0; // Reset buffer
        return false;
      }

      return true;
    }

    /*
    * Field handling
    */

    void PanasonicACWLAN::determine_mode(byte mode)
    {
      switch(mode) // Check mode
      {
        case 0x41: // Auto
          this->mode = climate::CLIMATE_MODE_HEAT_COOL;
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
          ESP_LOGW(ESPPAC::TAG, "Received unknown climate mode");
        break;
      }
    }

    std::string PanasonicACWLAN::determine_fan_speed(byte speed)
    {
      switch(speed)
      {
        case 0x32: // 1
          return "1";
        break;
        case 0x33: // 2
          return "2";
        break;
        case 0x34: // 3
          return "3";
        break;
        case 0x35: // 4
          return "4";
        case 0x36: // 5
          return "5";
        break;
        case 0x41: // Auto
          return "Automatic";
        break;
        default:
          ESP_LOGW(ESPPAC::TAG, "Received unknown fan speed");
          return "Unknown";
        break;
      }
    }

    std::string PanasonicACWLAN::determine_preset(byte preset)
    {
      switch(preset)
      {
        case 0x43: // Quiet
          return "Quiet";
        break;
        case 0x42: // Powerful
          return "Powerful";
        break;
        case 0x41: // Normal
          return "Normal";
        break;
        default:
          ESP_LOGW(ESPPAC::TAG, "Received unknown fan power");
          return "Normal";
        break;
      }
    }

    const char* PanasonicACWLAN::determine_swing_vertical(byte swing)
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
          ESP_LOGW(ESPPAC::TAG, "Received unknown vertical swing position");
          return "Unknown";
        break;
      }
    }

    const char* PanasonicACWLAN::determine_swing_horizontal(byte swing)
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
          ESP_LOGW(ESPPAC::TAG, "Received unknown horizontal swing position");
          return "Unknown";
        break;
      }
    }

    void PanasonicACWLAN::determine_swing(byte swing)
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
          ESP_LOGW(ESPPAC::TAG, "Received unknown swing mode");
        break;
      }
    }

    bool PanasonicACWLAN::determine_nanoex(byte nanoex)
    {
      switch(nanoex)
      {
        case 0x42:
          return false;
        default:
          return true;
      }
    }

    /*
    * Packet handling
    */

    void PanasonicACWLAN::handle_packet()
    {
      if(receiveBuffer[2] == 0x01 && receiveBuffer[3] == 0x01) // Ping
      {
        ESP_LOGD(ESPPAC::TAG, "Answering ping");
        send_command(ESPPAC::WLAN::CMD_PING, sizeof(ESPPAC::WLAN::CMD_PING), CommandType::Response);
      }
      else if(receiveBuffer[2] == 0x10 && receiveBuffer[3] == 0x89) // Received query response
      {
        ESP_LOGD(ESPPAC::TAG, "Received query response");

        if(receiveBufferIndex != 125)
        {
          ESP_LOGW(ESPPAC::TAG, "Received invalid query response");
          return;
        }

        if(receiveBuffer[14] == 0x31) // Check if power state is off
          this->mode = climate::CLIMATE_MODE_OFF; // Climate is off
        else
        {
          determine_mode(receiveBuffer[18]); // Check mode if power state is not off
        }

        update_target_temperature((int8_t)receiveBuffer[22]);
        update_current_temperature((int8_t)receiveBuffer[62]);
        update_outside_temperature((int8_t)receiveBuffer[66]); // Set current (outside) temperature

        const char* horizontalSwing = determine_swing_horizontal(receiveBuffer[34]);
        const char* verticalSwing = determine_swing_vertical(receiveBuffer[38]);

        update_swing_horizontal(horizontalSwing);
        update_swing_vertical(verticalSwing);

        bool nanoex = determine_nanoex(receiveBuffer[50]);

        update_nanoex(nanoex);

        this->custom_fan_mode = determine_fan_speed(receiveBuffer[26]);
        this->custom_preset = determine_preset(receiveBuffer[42]);

        determine_swing(receiveBuffer[30]);

        //climate::ClimateAction action = determine_action(); // Determine the current action of the AC
        //this->action = action;

        this->publish_state();
      }
      else if(receiveBuffer[2] == 0x10 && receiveBuffer[3] == 0x88) // Command ack
      {
        ESP_LOGV(ESPPAC::TAG, "Received command ack");
      }
      else if(receiveBuffer[2] == 0x10 && receiveBuffer[3] == 0x0A) // Report
      {
        ESP_LOGV(ESPPAC::TAG, "Received report");
        send_command(ESPPAC::WLAN::CMD_REPORT_ACK, sizeof(ESPPAC::WLAN::CMD_REPORT_ACK), CommandType::Response);

        if(receiveBufferIndex < 13)
        {
          ESP_LOGE(ESPPAC::TAG, "Report is too short to handle");
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
                  ESP_LOGV(ESPPAC::TAG, "Received power mode on");
                  // Ignore power on and let mode be set by other report
                break;
                case 0x31: // Power mode off
                  ESP_LOGV(ESPPAC::TAG, "Received power mode off");
                  this->mode = climate::CLIMATE_MODE_OFF;
                break;
                default:
                  ESP_LOGW(ESPPAC::TAG, "Received unknown power mode");
                break;
              }
            break;
            case 0xB0: // Mode
              determine_mode(receiveBuffer[currentIndex + 2]);
            break;
            case 0x31: // Target temperature
              ESP_LOGV(ESPPAC::TAG, "Received target temperature");
              update_target_temperature((int8_t)receiveBuffer[currentIndex + 2]);
            break;
            case 0xA0: // Fan speed
              ESP_LOGV(ESPPAC::TAG, "Received fan speed");
              this->custom_fan_mode = determine_fan_speed(receiveBuffer[currentIndex + 2]);
            case 0xB2:
              ESP_LOGV(ESPPAC::TAG, "Received preset");
              this->custom_preset = determine_preset(receiveBuffer[currentIndex + 2]);
            break;
            case 0xA1:
              ESP_LOGV(ESPPAC::TAG, "Received swing mode");
              determine_swing(receiveBuffer[currentIndex + 2]);
            break;
            case 0xA5: // Horizontal swing position
              ESP_LOGV(ESPPAC::TAG, "Received horizontal swing position");

              update_swing_horizontal(determine_swing_horizontal(receiveBuffer[currentIndex + 2]));
            break;
            case 0xA4: // Vertical swing position
              ESP_LOGV(ESPPAC::TAG, "Received vertical swing position");

              update_swing_vertical(determine_swing_vertical(receiveBuffer[currentIndex + 2]));
            break;
            case 0x33: // nanoex mode
              ESP_LOGV(ESPPAC::TAG, "Received nanoex state");

              update_nanoex(determine_nanoex(receiveBuffer[currentIndex + 2]));
            break;
            case 0x20:
              ESP_LOGV(ESPPAC::TAG, "Received unknown nanoex field");
              // Not sure what this one, ignore it for now
            break;
            default:
              ESP_LOGW(ESPPAC::TAG, "Report has unknown field");
            break;
          }
        }

        climate::ClimateAction action = determine_action(); // Determine the current action of the AC
        this->action = action;

        this->publish_state();
      }
      else if(receiveBuffer[2] == 0x01 && receiveBuffer[3] == 0x80) // Answer for handshake 16
      {
        ESP_LOGI(ESPPAC::TAG, "Panasonic AC component v%s initialized", ESPPAC::VERSION);
        state = ACState::Ready;
      }
      else
      {
        ESP_LOGW(ESPPAC::TAG, "Received unknown packet");
      }
    }

    void PanasonicACWLAN::handle_handshake_packet()
    {
      if(receiveBuffer[2] == 0x00 && receiveBuffer[3] == 0x89) // Answer for handshake 2
      {
        ESP_LOGD(ESPPAC::TAG, "Answering handshake [2/16]");
        send_command(ESPPAC::WLAN::CMD_HANDSHAKE_3, sizeof(ESPPAC::WLAN::CMD_HANDSHAKE_3));
      }
      else if(receiveBuffer[2] == 0x00 && receiveBuffer[3] == 0x8C) // Answer for handshake 3
      {
        ESP_LOGD(ESPPAC::TAG, "Answering handshake [3/16]");
        send_command(ESPPAC::WLAN::CMD_HANDSHAKE_4, sizeof(ESPPAC::WLAN::CMD_HANDSHAKE_4));
      }
      else if(receiveBuffer[2] == 0x00 && receiveBuffer[3] == 0x90) // Answer for handshake 4
      {
        ESP_LOGD(ESPPAC::TAG, "Answering handshake [4/16]");
        send_command(ESPPAC::WLAN::CMD_HANDSHAKE_5, sizeof(ESPPAC::WLAN::CMD_HANDSHAKE_5));
      }
      else if(receiveBuffer[2] == 0x00 && receiveBuffer[3] == 0x91) // Answer for handshake 5
      {
        ESP_LOGD(ESPPAC::TAG, "Answering handshake [5/16]");
        send_command(ESPPAC::WLAN::CMD_HANDSHAKE_6, sizeof(ESPPAC::WLAN::CMD_HANDSHAKE_6));
      }
      else if(receiveBuffer[2] == 0x00 && receiveBuffer[3] == 0x92) // Answer for handshake 6
      {
        ESP_LOGD(ESPPAC::TAG, "Answering handshake [6/16]");
        send_command(ESPPAC::WLAN::CMD_HANDSHAKE_7, sizeof(ESPPAC::WLAN::CMD_HANDSHAKE_7));
      }
      else if(receiveBuffer[2] == 0x00 && receiveBuffer[3] == 0xC1) // Answer for handshake 7
      {
        ESP_LOGD(ESPPAC::TAG, "Answering handshake [7/16]");
        send_command(ESPPAC::WLAN::CMD_HANDSHAKE_8, sizeof(ESPPAC::WLAN::CMD_HANDSHAKE_8));
      }
      else if(receiveBuffer[2] == 0x01 && receiveBuffer[3] == 0xCC) // Answer for handshake 8
      {
        ESP_LOGD(ESPPAC::TAG, "Answering handshake [8/16]");
        send_command(ESPPAC::WLAN::CMD_HANDSHAKE_9, sizeof(ESPPAC::WLAN::CMD_HANDSHAKE_9));
      }
      else if(receiveBuffer[2] == 0x10 && receiveBuffer[3] == 0x80) // Answer for handshake 9
      {
        ESP_LOGD(ESPPAC::TAG, "Answering handshake [9/16]");
        send_command(ESPPAC::WLAN::CMD_HANDSHAKE_10, sizeof(ESPPAC::WLAN::CMD_HANDSHAKE_10));
      }
      else if(receiveBuffer[2] == 0x10 && receiveBuffer[3] == 0x81) // Answer for handshake 10
      {
        ESP_LOGD(ESPPAC::TAG, "Answering handshake [10/16]");
        send_command(ESPPAC::WLAN::CMD_HANDSHAKE_11, sizeof(ESPPAC::WLAN::CMD_HANDSHAKE_11));
      }
      else if(receiveBuffer[2] == 0x00 && receiveBuffer[3] == 0x98) // Answer for handshake 11
      {
        ESP_LOGD(ESPPAC::TAG, "Answering handshake [11/16]");
        send_command(ESPPAC::WLAN::CMD_HANDSHAKE_12, sizeof(ESPPAC::WLAN::CMD_HANDSHAKE_12));
      }
      else if(receiveBuffer[2] == 0x01 && receiveBuffer[3] == 0x80) // Answer for handshake 12
      {
        ESP_LOGD(ESPPAC::TAG, "Answering handshake [12/16]");
        send_command(ESPPAC::WLAN::CMD_HANDSHAKE_13, sizeof(ESPPAC::WLAN::CMD_HANDSHAKE_13));
      }
      else if(receiveBuffer[2] == 0x10 && receiveBuffer[3] == 0x88) // Answer for handshake 13
      {
        // Ignore
        ESP_LOGD(ESPPAC::TAG, "Ignoring handshake [13/16]");
      }
      else if(receiveBuffer[2] == 0x01 && receiveBuffer[3] == 0x09) // First unsolicited packet from AC containing rx counter
      {
        ESP_LOGD(ESPPAC::TAG, "Received rx counter [14/16]");
        receivePacketCount = receiveBuffer[1]; // Set rx packet counter
        send_command(ESPPAC::WLAN::CMD_HANDSHAKE_14, sizeof(ESPPAC::WLAN::CMD_HANDSHAKE_14), CommandType::Response);
      }
      else if(receiveBuffer[2] == 0x00 && receiveBuffer[3] == 0x20) // Second unsolicited packet from AC
      {
        ESP_LOGD(ESPPAC::TAG, "Answering handshake [15/16]");
        state = ACState::FirstPoll; // Start delayed first poll
        send_command(ESPPAC::WLAN::CMD_HANDSHAKE_15, sizeof(ESPPAC::WLAN::CMD_HANDSHAKE_15), CommandType::Response);
      }
      else
      {
        ESP_LOGW(ESPPAC::TAG, "Received unknown packet during initialization");
      }
    }

    /*
    * Packet sending
    */

    void PanasonicACWLAN::send_set_command()
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

    void PanasonicACWLAN::send_command(const byte* command, size_t commandLength, CommandType type)
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

    void PanasonicACWLAN::send_packet(byte* packet, size_t packetLength, CommandType type)
    {
      byte checksum = ESPPAC::WLAN::HEADER; // Checksum is calculated by adding all bytes together
      packet[0] = ESPPAC::WLAN::HEADER; // Write header to packet

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
    void PanasonicACWLAN::handle_resend()
    {
      if(waitingForResponse && millis() - lastPacketSent > ESPPAC::WLAN::RESPONSE_TIMEOUT && receiveBufferIndex == 0) // Check if AC failed to respond in time and resend packet, if nothing was received yet
      {
        ESP_LOGD(ESPPAC::TAG, "Resending previous packet");
        send_command(lastCommand, lastCommandLength, CommandType::Resend);
      }
    }

    void PanasonicACWLAN::set_value(byte key, byte value)
    {
      if(setQueueIndex >= 15)
      {
        ESP_LOGE(ESPPAC::TAG, "Set queue overflow");
        setQueueIndex = 0;
        return;
      }

      setQueue[setQueueIndex][0] = key;
      setQueue[setQueueIndex][1] = value;
      setQueueIndex++;
    }

    /*
    * Sensor handling
    */

    void PanasonicACWLAN::set_vertical_swing_sensor(text_sensor::TextSensor *vertical_swing_sensor)
    {
      PanasonicAC::set_vertical_swing_sensor(vertical_swing_sensor);

      this->vertical_swing_sensor->add_on_state_callback([this](std::string value)
      {
        if(this->state != ACState::Ready)
          return;

        if(value != this->vertical_swing_state) // Ignore if already the correct state
        {
          ESP_LOGD(ESPPAC::TAG, "Setting vertical swing position");

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

    void PanasonicACWLAN::set_horizontal_swing_sensor(text_sensor::TextSensor *horizontal_swing_sensor)
    {
      PanasonicAC::set_horizontal_swing_sensor(horizontal_swing_sensor);

      this->horizontal_swing_sensor->add_on_state_callback([this](std::string value)
      {
        if(this->state != ACState::Ready)
          return;

        if(value != this->horizontal_swing_state) // Ignore if already the correct state
        {
          ESP_LOGD(ESPPAC::TAG, "Setting horizontal swing position");

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

    void PanasonicACWLAN::set_nanoex_switch(switch_::Switch *nanoex_switch)
    {
      PanasonicAC::set_nanoex_switch(nanoex_switch);

      this->nanoex_switch->add_on_state_callback([this](bool value)
      {
        if(this->state != ACState::Ready)
          return;

        if(value != this->nanoex_state) // Ignore if already the correct state
        {
          if(value)
          {
            ESP_LOGV(ESPPAC::TAG, "Turning nanoex on");
            set_value(0x33, 0x45); // nanoeX on
          }
          else
          {
            ESP_LOGV(ESPPAC::TAG, "Turning nanoex off");
            set_value(0x33, 0x42); // nanoeX off
          }

          send_set_command();
        }
      });
    }
  }
}
