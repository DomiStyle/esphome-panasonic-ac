#include "esppac_cnt.h"
#include "esppac_commands_cnt.h"
// TODO: Switch to CNT commands

namespace ESPPAC
{
  namespace CNT
  {
    PanasonicACCNT::PanasonicACCNT(uart::UARTComponent *uartComponent) : ESPPAC::PanasonicAC(uartComponent) {}

    void PanasonicACCNT::setup()
    {
      PanasonicAC::setup();

      ESP_LOGD(ESPPAC::TAG, "Using CZ-TACG1 protocol via CN-CNT");
    }

    void PanasonicACCNT::loop()
    {
      PanasonicAC::read_data();

      if(millis() - lastRead > ESPPAC::READ_TIMEOUT && receiveBufferIndex != 0) // Check if our read timed out and we received something
      {
        log_packet(receiveBuffer, receiveBufferIndex);

        if(!verify_packet()) // Verify length, header, counter and checksum
          return;

        waitingForResponse = false;
        lastPacketReceived = millis(); // Set the time at which we received our last packet

        handle_packet();

        receiveBufferIndex = 0; // Reset buffer
      }

      handle_poll(); // Handle sending poll packets
    }

    /*
    * ESPHome control request
    */

    void PanasonicACCNT::control(const climate::ClimateCall &call)
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
            this->data[0] = this->data[0] & 0xF0; // Strip right nib to turn AC off
          break;
          default:
            ESP_LOGV(ESPPAC::TAG, "Unsupported mode requested");
          break;
        }
      }

      if(call.get_target_temperature().has_value())
      {
        this->data[1] = *call.get_target_temperature() * 2;
      }

      if(call.get_custom_fan_mode().has_value())
      {
        ESP_LOGV(ESPPAC::TAG, "Requested fan mode change");

        std::string fanMode = *call.get_custom_fan_mode();

        if(fanMode.compare("Auto") == 0)
          this->data[3] = 0xA0;
        else if(fanMode.compare("1") == 0)
          this->data[3] = 0x30;
        else if(fanMode.compare("2") == 0)
          this->data[3] = 0x40;
        else if(fanMode.compare("3") == 0)
          this->data[3] = 0x50;
        else if(fanMode.compare("4") == 0)
          this->data[3] = 0x60;
        else if(fanMode.compare("5") == 0)
          this->data[3] = 0x70;
        else
          ESP_LOGV(ESPPAC::TAG, "Unsupported fan mode requested");
      }

      if(call.get_swing_mode().has_value())
      {
        ESP_LOGV(ESPPAC::TAG, "Requested swing mode change");

        switch(*call.get_swing_mode())
        {
          case climate::CLIMATE_SWING_BOTH:
            this->data[4] = 0xFD;
          break;
          case climate::CLIMATE_SWING_OFF:
            this->data[4] = 0x36; // Reset both to center
          break;
          case climate::CLIMATE_SWING_VERTICAL:
            this->data[4] = 0xF6; // Swing vertical, horizontal center
          break;
          case climate::CLIMATE_SWING_HORIZONTAL:
            this->data[4] = 0x3D; // Swing horizontal, vertical center
          break;
          default:
            ESP_LOGV(ESPPAC::TAG, "Unsupported swing mode requested");
          break;
        }
      }

/*
      if(call.get_preset().has_value())
      {
        ESP_LOGV(ESPPAC::TAG, "Requested preset change");

        switch(*call.get_preset())
        {
          case climate::CLIMATE_PRESET_NONE:
            this->data[5] = (this->data[5] & 0xF0); // Clear right nib for normal mode
          break;
          case climate::CLIMATE_PRESET_BOOST:
            this->data[5] = (this->data[5] & 0xF0) + 0x02; // Clear right nib and set powerful mode
          break;
          case climate::CLIMATE_PRESET_ECO:
            this->data[5] = (this->data[5] & 0xF0) + 0x04; // Clear right nib and set quiet mode
          break;
          default:
            ESP_LOGV(ESPPAC::TAG, "Unsupported preset requested");
          break;
        }
      }
*/

      if(call.get_custom_preset().has_value())
      {
        ESP_LOGV(ESPPAC::TAG, "Requested preset change");

        std::string preset = *call.get_custom_preset();

        if(preset.compare("Normal") == 0)
          this->data[5] = (this->data[5] & 0xF0); // Clear right nib for normal mode
        else if(preset.compare("Powerful") == 0)
          this->data[5] = (this->data[5] & 0xF0) + 0x02; // Clear right nib and set powerful mode
        else if(preset.compare("Quiet") == 0)
          this->data[5] = (this->data[5] & 0xF0) + 0x04; // Clear right nib and set quiet mode
        else
          ESP_LOGV(ESPPAC::TAG, "Unsupported preset requested");
      }

      send_command(this->data, 10, CommandType::Normal, ESPPAC::CNT::CTRL_HEADER);
      set_data(false);
    }

    /*
     * Set the data array to the fields
     */
    void PanasonicACCNT::set_data(bool set)
    {
      climate::ClimateMode mode = determine_mode(data[0]);
      std::string fanSpeed = determine_fan_speed(data[3]);
      //climate::ClimateFanMode fanSpeed = determine_fan_speed(data[3]);

      const char* verticalSwing = determine_vertical_swing(data[4]);
      const char* horizontalSwing = determine_horizontal_swing(data[4]);

      std::string preset = determine_preset(data[5]);
      bool nanoex = determine_preset_nanoex(data[5]);
      bool eco = determine_eco(data[8]);
      bool mildDry = determine_mild_dry(data[2]);

      this->mode = mode;
      //this->action = determine_action();

      this->custom_fan_mode = fanSpeed;
      //this->fan_mode = fanSpeed;

      this->update_target_temperature((int8_t)data[1]);

      if(set) // Also set current and outside temperature
      {
        this->update_current_temperature((int8_t)receiveBuffer[18]);
        this->update_outside_temperature((int8_t)receiveBuffer[19]);
      }

      if(strcmp(verticalSwing, "auto") == 0 && strcmp(horizontalSwing, "auto") == 0)
        this->swing_mode = climate::CLIMATE_SWING_BOTH;
      else if(strcmp(verticalSwing, "auto") == 0)
        this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
      else if(strcmp(horizontalSwing, "auto") == 0)
        this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
      else
        this->swing_mode = climate::CLIMATE_SWING_OFF;

      this->update_swing_vertical(verticalSwing);
      this->update_swing_horizontal(horizontalSwing);

      //this->preset = preset;
      this->custom_preset = preset;

      this->update_nanoex(nanoex);
      this->update_eco(eco);
      this->update_mild_dry(mildDry);
    }

    /*
     * Send a command, attaching header, packet length and checksum
     */
    void PanasonicACCNT::send_command(const byte* command, size_t commandLength, CommandType type, byte header = ESPPAC::CNT::CTRL_HEADER)
    {
      byte packet[commandLength + 3]; // Reserve space for upcoming packet

      packet[0] = header;
      packet[1] = commandLength;

      //memcpy(&packet[2], &command, commandLength); // Copy command to packet

      byte checksum = 0;

      checksum -= header;
      checksum -= commandLength;

      for(int i = 2; i < commandLength + 2; i++)
      {
        packet[i] = command[i - 2]; // Copy command to packet
        checksum -= packet[i]; // Add to checksum
      }

      packet[commandLength + 2] = checksum;

      //lastCommand = command; // Store the last command we sent
      //lastCommandLength = commandLength; // Store the length of the last command we sent

      send_packet(packet, commandLength + 3, type); // Actually send the constructed packet
    }

    /*
     * Send a raw packet, as is
     */
    void PanasonicACCNT::send_packet(byte* packet, size_t packetLength, CommandType type)
    {
      lastPacketSent = millis(); // Save the time when we sent the last packet

      if(type != CommandType::Response) // Don't wait for a response for responses
        waitingForResponse = true; // Mark that we are waiting for a response

      write_array(packet, packetLength); // Write to UART
      log_packet(packet, packetLength, true); // Write to log
    }

    /*
    * Loop handling
    */

    void PanasonicACCNT::handle_poll()
    {
      if(millis() - lastPacketSent > ESPPAC::CNT::POLL_INTERVAL)
      {
        ESP_LOGV(ESPPAC::TAG, "Polling AC");
        send_command(ESPPAC::CNT::CMD_POLL, sizeof(ESPPAC::CNT::CMD_POLL), CommandType::Normal, ESPPAC::CNT::POLL_HEADER);
      }
    }

    /*
    * Packet handling
    */

    bool PanasonicACCNT::verify_packet()
    {
      if(receiveBufferIndex < 12)
      {
        ESP_LOGW(ESPPAC::TAG, "Dropping invalid packet (length)");

        receiveBufferIndex = 0; // Reset buffer
        return false;
      }

      if(receiveBuffer[0] != ESPPAC::CNT::CTRL_HEADER && receiveBuffer[0] != ESPPAC::CNT::POLL_HEADER) // Check if header matches
      {
        ESP_LOGW(ESPPAC::TAG, "Dropping invalid packet (header)");

        receiveBufferIndex = 0; // Reset buffer
        return false;
      }

      if(receiveBuffer[1] != receiveBufferIndex - 3) // Packet length minus header, packet length and checksum
      {
        ESP_LOGD(ESPPAC::TAG, "Dropping invalid packet (length mismatch)");

        receiveBufferIndex = 0; // Reset buffer
        return false;
      }

      byte checksum = 0;

      for(int i = 0; i < receiveBufferIndex; i++)
      {
        checksum += receiveBuffer[i];
      }

      if(checksum != 0)
      {
        ESP_LOGD(ESPPAC::TAG, "Dropping invalid packet (checksum)");

        receiveBufferIndex = 0; // Reset buffer
        return false;
      }

      return true;
    }

    void PanasonicACCNT::handle_packet()
    {
      if(receiveBuffer[0] == ESPPAC::CNT::POLL_HEADER)
      {
        memcpy(data, &receiveBuffer[2], 10); // Copy 10 data bytes from query response to data

        this->set_data(true);
        this->publish_state();

        if(this->state != ACState::Ready)
          this->state = ACState::Ready; // Mark as ready after first poll
      }
      else
      {
        ESP_LOGD(ESPPAC::TAG, "Received unknown packet");
      }
    }

    climate::ClimateMode PanasonicACCNT::determine_mode(byte mode)
    {
      byte nib1 = (mode >> 4) & 0x0F; // Left nib for mode
      byte nib2 = (mode >> 0) & 0x0F; // Right nib for power state

      if(nib2 == 0x00)
        return climate::CLIMATE_MODE_OFF;

      switch(nib1)
      {
        case 0x00: // Auto
          return climate::CLIMATE_MODE_HEAT_COOL;
        case 0x03: // Cool
          return climate::CLIMATE_MODE_COOL;
        case 0x04: // Heat
          return climate::CLIMATE_MODE_HEAT;
        case 0x02: // Dry
          return climate::CLIMATE_MODE_DRY;
        case 0x06: // Fan only
          return climate::CLIMATE_MODE_FAN_ONLY;
        default:
          ESP_LOGW(ESPPAC::TAG, "Received unknown climate mode");
          return climate::CLIMATE_MODE_OFF;
      }
    }

    std::string PanasonicACCNT::determine_fan_speed(byte speed)
    //climate::ClimateFanMode PanasonicACCNT::determine_fan_speed(byte speed)
    {
      switch(speed)
      {
        case 0xA0: // Auto
          return "Auto";
          //return climate::CLIMATE_FAN_AUTO;
        case 0x30: // 1
          return "1";
          //return climate::CLIMATE_FAN_LOW;
        case 0x40: // 2
          return "2";
          //return climate::CLIMATE_FAN_MEDIUM;
        case 0x50: // 3
          return "3";
          //return climate::CLIMATE_FAN_MIDDLE;
        case 0x60: // 4
          return "4";
          //return climate::CLIMATE_FAN_HIGH;
        case 0x70: // 5
          return "5";
          //return climate::CLIMATE_FAN_FOCUS;
        default:
          ESP_LOGW(ESPPAC::TAG, "Received unknown fan speed");
          return "Unknown";
          //return climate::CLIMATE_FAN_OFF;
      }
    }

    const char* PanasonicACCNT::determine_vertical_swing(byte swing)
    {
      byte nib = (swing >> 4) & 0x0F; // Left nib for vertical swing

      switch(nib)
      {
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
          ESP_LOGW(ESPPAC::TAG, "Received unknown vertical swing mode");
          return "Unknown";
      }
    }

    const char* PanasonicACCNT::determine_horizontal_swing(byte swing)
    {
      byte nib = (swing >> 0) & 0x0F; // Right nib for horizontal swing

      switch(nib)
      {
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
          ESP_LOGW(ESPPAC::TAG, "Received unknown horizontal swing mode");
          return "Unknown";
      }
    }

    std::string PanasonicACCNT::determine_preset(byte preset)
    {
      byte nib = (preset >> 0) & 0x0F; // Right nib for preset (powerful/quiet)

      switch(nib)
      {
        case 0x02:
          //return climate::CLIMATE_PRESET_BOOST;
          return "Powerful";
        case 0x04:
          //return climate::CLIMATE_PRESET_ECO;
          return "Quiet";
        case 0x00:
          //return climate::CLIMATE_PRESET_NONE;
          return "Normal";
        default:
          ESP_LOGW(ESPPAC::TAG, "Received unknown preset");
          //return climate::CLIMATE_PRESET_NONE;
          return "Normal";
      }
    }

    bool PanasonicACCNT::determine_preset_nanoex(byte preset)
    {
      byte nib = (preset >> 4) & 0x0F; // Left nib for nanoex

      if(nib == 0x04)
        return true;
      else if(nib == 0x00)
        return false;
      else
      {
        ESP_LOGW(ESPPAC::TAG, "Received unknown nanoex value");
        return false;
      }
    }

    bool PanasonicACCNT::determine_eco(byte value)
    {
      if(value == 0x40)
        return true;
      else if(value == 0x00)
        return false;
      else
      {
        ESP_LOGW(ESPPAC::TAG, "Received unknown eco value");
        return false;
      }
    }

    bool PanasonicACCNT::determine_mild_dry(byte value)
    {
      if(value == 0x7F)
        return true;
      else if(value == 0x80)
        return false;
      else
      {
        ESP_LOGW(ESPPAC::TAG, "Received unknown mild dry value");
        return false;
      }
    }

    /*
    * Sensor handling
    */

    void PanasonicACCNT::set_vertical_swing_sensor(text_sensor::TextSensor *vertical_swing_sensor)
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
            this->data[4] = (this->data[4] & 0x0F) + 0x50;
          else if(value == "down_center")
            this->data[4] = (this->data[4] & 0x0F) + 0x40;
          else if(value == "center")
            this->data[4] = (this->data[4] & 0x0F) + 0x30;
          else if(value == "up_center")
            this->data[4] = (this->data[4] & 0x0F) + 0x20;
          else if(value == "up")
            this->data[4] = (this->data[4] & 0x0F) + 0x10;

          send_command(this->data, 10, CommandType::Normal, ESPPAC::CNT::CTRL_HEADER);
        }
      });
    }

    void PanasonicACCNT::set_horizontal_swing_sensor(text_sensor::TextSensor *horizontal_swing_sensor)
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
            this->data[4] = (this->data[4] & 0xF0) + 0x09;
          else if(value == "left_center")
            this->data[4] = (this->data[4] & 0xF0) + 0x0A;
          else if(value == "center")
            this->data[4] = (this->data[4] & 0xF0) + 0x06;
          else if(value == "right_center")
            this->data[4] = (this->data[4] & 0xF0) + 0x0B;
          else if(value == "right")
            this->data[4] = (this->data[4] & 0xF0) + 0x0C;

          send_command(this->data, 10, CommandType::Normal, ESPPAC::CNT::CTRL_HEADER);
        }
      });
    }

    void PanasonicACCNT::set_nanoex_switch(switch_::Switch *nanoex_switch)
    {
      PanasonicAC::set_nanoex_switch(nanoex_switch);

      this->nanoex_switch->add_on_state_callback([this](bool value)
      {
        if(this->state != ACState::Ready)
          return;

        if(value != this->nanoex_state) // Ignore if already the correct state
        {
          this->nanoex_state = value;

          if(value)
          {
            ESP_LOGV(ESPPAC::TAG, "Turning nanoex on");
            this->data[5] = (this->data[5] & 0x0F) + 0x40;
          }
          else
          {
            ESP_LOGV(ESPPAC::TAG, "Turning nanoex off");
            this->data[5] = (this->data[5] & 0x0F);
          }

          send_command(this->data, 10, CommandType::Normal, ESPPAC::CNT::CTRL_HEADER);
        }
      });
    }

    void PanasonicACCNT::set_eco_switch(switch_::Switch *eco_switch)
    {
      PanasonicAC::set_eco_switch(eco_switch);

      this->eco_switch->add_on_state_callback([this](bool value)
      {
        if(this->state != ACState::Ready)
          return;

        if(value != this->eco_state) // Ignore if already the correct state
        {
          this->eco_state = value;

          if(value)
          {
            ESP_LOGV(ESPPAC::TAG, "Turning eco mode on");
            this->data[8] = 0x40;
          }
          else
          {
            ESP_LOGV(ESPPAC::TAG, "Turning eco mode off");
            this->data[8] = 0x00;
          }

          send_command(this->data, 10, CommandType::Normal, ESPPAC::CNT::CTRL_HEADER);
        }
      });
    }

    void PanasonicACCNT::set_mild_dry_switch(switch_::Switch *mild_dry_switch)
    {
      PanasonicAC::set_mild_dry_switch(mild_dry_switch);

      this->mild_dry_switch->add_on_state_callback([this](bool value)
      {
        if(this->state != ACState::Ready)
          return;

        if(value != this->mild_dry_state) // Ignore if already the correct state
        {
          this->mild_dry_state = value;

          if(value)
          {
            ESP_LOGV(ESPPAC::TAG, "Turning mild dry on");
            this->data[2] = 0x7F;
          }
          else
          {
            ESP_LOGV(ESPPAC::TAG, "Turning mild dry off");
            this->data[2] = 0x80;
          }

          send_command(this->data, 10, CommandType::Normal, ESPPAC::CNT::CTRL_HEADER);
        }
      });
    }
  }
}
