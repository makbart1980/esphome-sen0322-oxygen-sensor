#include "sen0322.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome {
namespace sen0322 {

static const char *const TAG = "sen0322";

static constexpr uint8_t OXYGEN_DATA_REGISTER = 0x03;
static constexpr uint8_t CALIBRATION_REGISTER = 0x0A;
static constexpr uint8_t PROBE_LIFE_REGISTER = 0x0E;
static constexpr uint8_t VERSION_REGISTER = 0x0F;

void SEN0322Sensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SEN0322...");

  this->measurement_pending_ = false;
  this->status_set_warning();
  this->update();
}

void SEN0322Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "SEN0322 Oxygen Sensor (diagnostics v1):");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Oxygen", this);
}

void SEN0322Sensor::update() {
  if (this->measurement_pending_) {
    return;
  }

  this->measurement_pending_ = true;

  // Read-only diagnostics using registers from the DFRobot library.
  // Sending one register address selects it without changing calibration.
  const auto read_status_register =
      [this](uint8_t reg, uint8_t &value) -> bool {
    if (this->write(&reg, 1) != i2c::ERROR_OK) {
      return false;
    }

    return this->read(&value, 1) == i2c::ERROR_OK;
  };

  uint8_t version = 0;

  if (read_status_register(VERSION_REGISTER, version)) {
    ESP_LOGD(TAG, "Version register 0x0F: 0x%02X", version);

    if (version == 0x01) {
      uint8_t probe_life = 0;

      if (read_status_register(PROBE_LIFE_REGISTER, probe_life)) {
        ESP_LOGD(TAG, "Probe life register 0x0E: 0x%02X",
                 probe_life);

        if (probe_life == 0x00) {
          ESP_LOGW(TAG, "Module reports an exhausted oxygen probe");
        } else if (probe_life == 0x01) {
          ESP_LOGD(TAG, "Module reports normal probe life");
        } else {
          ESP_LOGW(TAG, "Unrecognized probe life status");
        }
      } else {
        ESP_LOGW(TAG, "Could not read probe life status");
      }
    } else if (version == 0xFF) {
      ESP_LOGD(TAG, "Older module: probe life status is not supported");
    } else {
      ESP_LOGW(TAG,
               "Unrecognized module version; skipping probe life check");
    }
  } else {
    ESP_LOGW(TAG,
             "Version register unavailable; continuing measurement");
  }

  // Select the calibration register without an extra data byte.
  const uint8_t reg = CALIBRATION_REGISTER;

  if (this->write(&reg, 1) != i2c::ERROR_OK) {
    this->handle_error_("Failed to select calibration register");
    return;
  }

  // Recover if a measurement does not finish.
  this->set_timeout("measurement_timeout", 1000, [this]() {
    this->handle_error_("Measurement timed out");
  });

  // Allow the sensor to prepare its calibration data.
  this->set_timeout("calibration_read", 50, [this]() {
    this->read_calibration_();
  });
}

void SEN0322Sensor::read_calibration_() {
  if (!this->measurement_pending_) {
    return;
  }

  uint8_t data[2] = {};

  if (this->read(data, sizeof(data)) != i2c::ERROR_OK) {
    this->handle_error_("Failed to read calibration data");
    return;
  }

  // Decode the calibration coefficient as a little-endian integer.
  const uint16_t raw_calibration =
      static_cast<uint16_t>(data[0]) |
      (static_cast<uint16_t>(data[1]) << 8);

  if (raw_calibration == 0xFFFF) {
    this->handle_error_("Invalid calibration data");
    return;
  }

  // Use the manufacturer's fallback when the stored value is zero.
  this->calibration_factor_ =
      raw_calibration == 0
          ? 20.9f / 120.0f
          : static_cast<float>(raw_calibration) / 1000.0f;

  ESP_LOGD(TAG,
           "Calibration bytes: %02X %02X; raw: %u; factor: %.6f",
           data[0], data[1],
           static_cast<unsigned>(raw_calibration),
           this->calibration_factor_);

  // Select the oxygen data register.
  const uint8_t reg = OXYGEN_DATA_REGISTER;

  if (this->write(&reg, 1) != i2c::ERROR_OK) {
    this->handle_error_("Failed to select oxygen data register");
    return;
  }

  // Wait without blocking the ESPHome main loop.
  this->set_timeout("oxygen_read", 100, [this]() {
    this->read_oxygen_();
  });
}

void SEN0322Sensor::read_oxygen_() {
  if (!this->measurement_pending_) {
    return;
  }

  uint8_t data[3] = {};

  if (this->read(data, sizeof(data)) != i2c::ERROR_OK) {
    this->handle_error_("Failed to read oxygen data");
    return;
  }

  // Decode the three bytes according to the DFRobot library.
  const float raw_value =
      static_cast<float>(data[0]) +
      static_cast<float>(data[1]) / 10.0f +
      static_cast<float>(data[2]) / 100.0f;

  const float oxygen =
      this->calibration_factor_ * raw_value;

  ESP_LOGD(TAG, "Raw data: %02X %02X %02X; raw signal: %.4f",
           data[0], data[1], data[2], raw_value);

  // Preserve zero and low readings; reject invalid or unsupported values.
  if (!std::isfinite(oxygen) || oxygen < 0.0f || oxygen > 30.0f) {
    ESP_LOGW(TAG, "Oxygen reading outside supported range: %.4f%%",
             oxygen);

    this->handle_error_("Invalid oxygen reading");
    return;
  }

  this->cancel_timeout("measurement_timeout");
  this->measurement_pending_ = false;
  this->status_clear_warning();

  ESP_LOGD(TAG, "Oxygen concentration: %.4f%%", oxygen);
  this->publish_state(oxygen);
}

void SEN0322Sensor::handle_error_(const char *message) {
  ESP_LOGW(TAG, "%s", message);

  this->cancel_timeout("measurement_timeout");
  this->cancel_timeout("calibration_read");
  this->cancel_timeout("oxygen_read");

  this->measurement_pending_ = false;
  this->status_set_warning();

  // Publish an invalid measurement instead of retaining an old value.
  this->publish_state(NAN);
}

float SEN0322Sensor::get_setup_priority() const {
  return setup_priority::DATA;
}

}  // namespace sen0322
}  // namespace esphome
