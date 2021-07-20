#include "esphome.h"
#include "esppac.h"

using namespace esphome;

namespace ESPPAC
{
    PanasonicAC::PanasonicAC(uart::UARTComponent *parent) : uart::UARTDevice(parent) {}

    void PanasonicAC::control(const climate::ClimateCall &call)
    {
        ESP_LOGV(ESPPAC::TAG, "AC control request");
    }

    climate::ClimateTraits PanasonicAC::traits()
    {
      auto traits = climate::ClimateTraits();

      traits.set_supports_action(true);

      traits.set_supports_current_temperature(true);
      traits.set_supports_two_point_target_temperature(false);
      traits.set_visual_min_temperature(MIN_TEMPERATURE);
      traits.set_visual_max_temperature(MAX_TEMPERATURE);
      traits.set_visual_temperature_step(TEMPERATURE_STEP);

      traits.set_supported_modes(
      {
        climate::CLIMATE_MODE_OFF,
        climate::CLIMATE_MODE_HEAT_COOL,
        climate::CLIMATE_MODE_COOL,
        climate::CLIMATE_MODE_HEAT,
        climate::CLIMATE_MODE_FAN_ONLY,
        climate::CLIMATE_MODE_DRY
      });

      traits.set_supported_fan_modes(
      {
        climate::CLIMATE_FAN_AUTO,
        climate::CLIMATE_FAN_LOW,
        climate::CLIMATE_FAN_MEDIUM,
        climate::CLIMATE_FAN_HIGH,
        climate::CLIMATE_FAN_MIDDLE,
        climate::CLIMATE_FAN_FOCUS
      });

      traits.set_supported_swing_modes(
      {
        climate::CLIMATE_SWING_OFF,
        climate::CLIMATE_SWING_BOTH,
        climate::CLIMATE_SWING_VERTICAL,
        climate::CLIMATE_SWING_HORIZONTAL
      });

      traits.set_supported_presets(
      {
        climate::CLIMATE_PRESET_BOOST, // Powerful
        climate::CLIMATE_PRESET_ECO // Quiet
      });

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

        ESP_LOGI(ESPPAC::TAG, "Panasonic AC component v%s starting...", ESPPAC::VERSION);
    }

    void PanasonicAC::loop()
    {
        read_data(); // Read data from UART (if there is any)
    }

    void PanasonicAC::read_data()
    {
      while(available()) // Read while data is available
      {
        if(receiveBufferIndex >= ESPPAC::BUFFER_SIZE)
        {
          ESP_LOGE(ESPPAC::TAG, "Receive buffer overflow");
          receiveBufferIndex = 0;
        }

        receiveBuffer[receiveBufferIndex] = read(); // Store in receive buffer

        lastRead = millis(); // Update lastRead timestamp
        receiveBufferIndex++; // Increase buffer index
      }
    }

    const char* PanasonicAC::get_swing_vertical(byte swing, ACType type = ACType::DNSKP11)
    {
      if(type == ACType::DNSKP11)
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
            return "unknown";
          break;
        }
      }

      ESP_LOGE(ESPPAC::TAG, "Received unknown AC type for vertical swing");
      return "unknown";
    }

    const char* PanasonicAC::get_swing_horizontal(byte swing, ACType type = ACType::DNSKP11)
    {
      if(type == ACType::DNSKP11)
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
            return "unknown";
          break;
        }
      }

      ESP_LOGE(ESPPAC::TAG, "Received unknown AC type for horizontal swing");
      return "unknown";
    }

    void PanasonicAC::update_outside_temperature(int8_t temperature)
    {
      if(temperature > ESPPAC::TEMPERATURE_THRESHOLD)
      {
          ESP_LOGW(ESPPAC::TAG, "Received out of range outside temperature");
          return;
      }

      if(this->outside_temperature_sensor != NULL && this->outside_temperature_sensor->state != temperature)
        this->outside_temperature_sensor->publish_state(temperature); // Set current (outside) temperature; no temperature steps
    }

    void PanasonicAC::update_current_temperature(int8_t temperature)
    {
      if(temperature > ESPPAC::TEMPERATURE_THRESHOLD)
      {
          ESP_LOGW(ESPPAC::TAG, "Received out of range inside temperature");
          return;
      }

      this->current_temperature = temperature;
    }

    void PanasonicAC::update_target_temperature(int8_t temperature)
    {
      temperature = temperature * ESPPAC::TEMPERATURE_STEP;

      if(temperature > ESPPAC::TEMPERATURE_THRESHOLD)
      {
          ESP_LOGW(ESPPAC::TAG, "Received out of range target temperature");
          return;
      }

      this->target_temperature = temperature;
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
    }

    void PanasonicAC::set_horizontal_swing_sensor(text_sensor::TextSensor *horizontal_swing_sensor)
    {
      this->horizontal_swing_sensor = horizontal_swing_sensor;
    }

    void PanasonicAC::set_nanoex_switch(switch_::Switch *nanoex_switch)
    {
      this->nanoex_switch = nanoex_switch;
    }

    /*
     * Debugging
     */

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

      ESP_LOGV(ESPPAC::TAG, buffer);
    }
}
