#pragma once

#include "esphome/components/climate/climate_traits.h"

namespace esphome {
namespace panasonic_ac {

static const uint8_t MIN_TEMPERATURE = 16;     // Minimum temperature as reported by Panasonic app
static const uint8_t MAX_TEMPERATURE = 30;     // Maximum temperature as supported by Panasonic app
static const float TEMPERATURE_STEP = 0.5;     // Steps the temperature can be set in
static const float TEMPERATURE_TOLERANCE = 2;  // The tolerance to allow when checking the climate state
static const uint8_t TEMPERATURE_THRESHOLD = 100;  // Maximum temperature the AC can report before considering the temperature as invalid

static const std::string FAN_SPEED_LEVEL_AUTO = "Auto "; // HACK: setting to "Auto" (without trailing space) won't work because it will be parsed as built-in CLIMATE_FAN_MODE_AUTO
static const std::string FAN_SPEED_LEVEL_1 = "1";
static const std::string FAN_SPEED_LEVEL_2 = "2";
static const std::string FAN_SPEED_LEVEL_3 = "3";
static const std::string FAN_SPEED_LEVEL_4 = "4";
static const std::string FAN_SPEED_LEVEL_5 = "5";

static const std::string PRESET_NONE = "None "; // HACK: setting to "None" (without trailing space) won't work because it will be parsed as built-in CLIMATE_PRESET_NONE
static const std::string PRESET_QUIET = "Quiet";
static const std::string PRESET_POWERFUL = "Powerful";

class PanasonicACTraits: public climate::ClimateTraits {
    public:
        void set_default_traits() {
            // Enable actions support
            this->set_supports_action(true);
            // Enable current temperature supports
            this->set_supports_current_temperature(true);
            // Disable two point target temperature support
            this->set_supports_two_point_target_temperature(false);
            // Set visual temperature parameters
            this->set_visual_min_temperature(MIN_TEMPERATURE);
            this->set_visual_max_temperature(MAX_TEMPERATURE);
            this->set_visual_temperature_step(TEMPERATURE_STEP);
            // Set default supported modes
            this->set_supported_modes({
                climate::CLIMATE_MODE_OFF,
                climate::CLIMATE_MODE_HEAT,
                climate::CLIMATE_MODE_COOL,
                climate::CLIMATE_MODE_HEAT_COOL,
                // climate::CLIMATE_MODE_DRY, TODO: do i need this?
                climate::CLIMATE_MODE_FAN_ONLY
            });
            // Set default supported swing modes
            this->set_supported_swing_modes({
                climate::CLIMATE_SWING_OFF,
                climate::CLIMATE_SWING_VERTICAL
            });
            // Set default custom presets
            this->set_supported_custom_presets({
                PRESET_NONE,
                PRESET_QUIET,
                PRESET_POWERFUL
            });
            // Set default fan speed levels
            this->set_supported_custom_fan_modes({
                FAN_SPEED_LEVEL_AUTO,
                FAN_SPEED_LEVEL_1,
                FAN_SPEED_LEVEL_2,
                FAN_SPEED_LEVEL_3,
                FAN_SPEED_LEVEL_4,
                FAN_SPEED_LEVEL_5,
            });
        }
};

}
}