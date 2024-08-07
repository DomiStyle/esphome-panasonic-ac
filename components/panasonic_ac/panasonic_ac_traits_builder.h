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

class PanasonicACTraitsBuilder {
    public:
        PanasonicACTraitsBuilder();
        climate::ClimateTraits build_traits() const { return this->traits; };
        void add_horizontal_swing_mode();
        void add_vertical_swing_mode();
    private:
        climate::ClimateTraits traits = climate::ClimateTraits();
};

}
}