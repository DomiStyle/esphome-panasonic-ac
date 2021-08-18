#include <vector>

namespace esphome {
namespace panasonic_ac {
namespace CNT {

/*
 * Poll command
 */

static const std::vector<uint8_t> CMD_POLL = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/*
 * Control command
 */

/*
      static const byte CMD_TEMPERATURE[]
      {
        0x34,
        0xFF, // Temperature * 2
        0x00,
        0xA0,
        0xFD,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00
      };

      static const byte CMD_FAN_SPEED[]
      {
        0x34,
        0x34,
        0x00,
        0xFF, // Fan speed
        0x3c,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00
      };

      // Same structure for vertical and horizontal swing
      static const byte CMD_SWING[]
      {
        0x34,
        0x34,
        0x00,
        0xA0,
        0xFF, // Swing
        0x00,
        0x00,
        0x00,
        0x00,
        0x00
      };
      */

}  // namespace CNT
}  // namespace panasonic_ac
}  // namespace esphome
