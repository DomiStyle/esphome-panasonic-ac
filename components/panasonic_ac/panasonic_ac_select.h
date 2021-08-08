#pragma once

#include "esphome/components/select/select.h"
#include "esphome/core/component.h"

namespace esphome {
namespace panasonic_ac {

class PanasonicACSelect : public select::Select, public Component {
 protected:
  void control(const std::string &value) override { this->publish_state(value); }
};

}  // namespace panasonic_ac
}  // namespace esphome
