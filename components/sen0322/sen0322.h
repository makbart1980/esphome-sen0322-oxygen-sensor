#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace sen0322 {

class SEN0322Sensor : public PollingComponent,
                      public i2c::I2CDevice,
                      public sensor::Sensor {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override;

 protected:
  void read_calibration_();
  void read_oxygen_();
  void handle_error_(const char *message);

  float calibration_factor_{20.9f / 120.0f};
  bool measurement_pending_{false};
};

}  // namespace sen0322
}  // namespace esphome
