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

        climate::ClimateMode mode = determine_mode(receiveBuffer[2]);
        std::string fanSpeed = determine_fan_speed(receiveBuffer[5]);

        const char* verticalSwing = determine_vertical_swing(receiveBuffer[6]);
        const char* horizontalSwing = determine_horizontal_swing(receiveBuffer[6]);

        climate::ClimatePreset preset = determine_preset(receiveBuffer[7]);
        bool nanoex = determine_preset_nanoex(receiveBuffer[7]);
        bool mildDry = determine_mild_dry(receiveBuffer[4]);

        this->mode = mode;
        this->action = determine_action();
        this->custom_fan_mode = fanSpeed;

        this->update_target_temperature((int8_t)receiveBuffer[3]);
        this->update_current_temperature((int8_t)receiveBuffer[18]);
        this->update_outside_temperature((int8_t)receiveBuffer[19]);

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

        this->preset = preset;

        this->update_nanoex(nanoex);
        //this->update_mild_dry(mildDry); // TODO: Implement mild dry

        this->publish_state();
      }
      else
      {
        ESP_LOGD(ESPPAC::TAG, "Received unknown packet");
      }
    }

    climate::ClimateMode PanasonicACCNT::determine_mode(byte mode)
    {
      switch(mode)
      {
        case 0x04: // Auto
          return climate::CLIMATE_MODE_HEAT_COOL;
        case 0x34: // Cool
          return climate::CLIMATE_MODE_COOL;
        case 0x44: // Heat
          return climate::CLIMATE_MODE_HEAT;
        case 0x24: // Dry
          return climate::CLIMATE_MODE_DRY;
        case 0x64: // Fan only
          return climate::CLIMATE_MODE_FAN_ONLY;
        case 0x30: // Off
          return climate::CLIMATE_MODE_OFF;
        default:
          ESP_LOGW(ESPPAC::TAG, "Received unknown climate mode");
          return climate::CLIMATE_MODE_OFF;
      }
    }

    std::string PanasonicACCNT::determine_fan_speed(byte speed)
    {
      switch(speed)
      {
        case 0xA0: // Auto
          return "auto";
        case 0x30: // 1
          return "lowest";
        case 0x40: // 2
          return "low";
        case 0x50: // 3
          return "medium";
        case 0x60: // 4
          return "high";
        case 0x70: // 5
          return "highest";
        default:
          ESP_LOGW(ESPPAC::TAG, "Received unknown fan speed");
          return "unknown";
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
        default:
          ESP_LOGW(ESPPAC::TAG, "Received unknown vertical swing mode");
          return "unknown";
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
        default:
          ESP_LOGW(ESPPAC::TAG, "Received unknown vertical swing mode");
          return "unknown";
      }
    }

    climate::ClimatePreset PanasonicACCNT::determine_preset(byte preset)
    {
      byte nib = (preset >> 0) & 0x0F; // Right nib for preset (powerful/quiet)

      switch(nib)
      {
        case 0x02:
          return climate::CLIMATE_PRESET_BOOST;
        case 0x04:
          return climate::CLIMATE_PRESET_ECO;
        case 0x00:
          return climate::CLIMATE_PRESET_NONE;
        default:
          ESP_LOGW(ESPPAC::TAG, "Received unknown preset");
          return climate::CLIMATE_PRESET_NONE;
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
  }
}
