#pragma once

namespace esphome {
namespace panasonic_ac {
namespace WLAN {

// A single property to request during a poll, or to set via a property-based command.
struct PollProperty {
  uint8_t attrb;
  uint8_t property;
};

enum class PollPropertiesMode { EXTEND, REPLACE };

}  // namespace WLAN
}  // namespace panasonic_ac
}  // namespace esphome