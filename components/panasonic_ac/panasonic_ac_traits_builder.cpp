#include "panasonic_ac_traits_builder.h"

namespace esphome {
namespace panasonic_ac {

PanasonicACTraitsBuilder::PanasonicACTraitsBuilder() {
    // Enable actions support
    this->traits.set_supports_action(true);
    // Enable current temperature supports
    this->traits.set_supports_current_temperature(true);
    // Disable two point target temperature support
    this->traits.set_supports_two_point_target_temperature(false);
    // Set visual temperature parameters
    this->traits.set_visual_min_temperature(MIN_TEMPERATURE);
    this->traits.set_visual_max_temperature(MAX_TEMPERATURE);
    this->traits.set_visual_temperature_step(TEMPERATURE_STEP);
    // Set default supported modes
    this->traits.set_supported_modes({
        climate::CLIMATE_MODE_OFF,
        climate::CLIMATE_MODE_HEAT,
        climate::CLIMATE_MODE_COOL,
        climate::CLIMATE_MODE_HEAT_COOL,
        climate::CLIMATE_MODE_FAN_ONLY
        // climate::CLIMATE_MODE_DRY, TODO: do i need this?
    });
    // Set default OFF swing mode
    this->traits.add_supported_swing_mode(climate::CLIMATE_SWING_OFF);
    // Set default custom presets
    this->traits.set_supported_custom_presets({
        PRESET_NONE,
        PRESET_QUIET,
        PRESET_POWERFUL
    });
    // Set default fan speed levels
    this->traits.set_supported_custom_fan_modes({
        FAN_SPEED_LEVEL_AUTO,
        FAN_SPEED_LEVEL_1,
        FAN_SPEED_LEVEL_2,
        FAN_SPEED_LEVEL_3,
        FAN_SPEED_LEVEL_4,
        FAN_SPEED_LEVEL_5,
    });
}

void PanasonicACTraitsBuilder::add_horizontal_swing_mode() {
    this->traits.add_supported_swing_mode(climate::CLIMATE_SWING_HORIZONTAL);
    if (this->traits.supports_swing_mode(climate::CLIMATE_SWING_VERTICAL)) {
        this->traits.add_supported_swing_mode(climate::CLIMATE_SWING_BOTH);
    }
}

void PanasonicACTraitsBuilder::add_vertical_swing_mode() {
    this->traits.add_supported_swing_mode(climate::CLIMATE_SWING_VERTICAL);
    if (this->traits.supports_swing_mode(climate::CLIMATE_SWING_HORIZONTAL)) {
        this->traits.add_supported_swing_mode(climate::CLIMATE_SWING_BOTH);
    }
}

}
}