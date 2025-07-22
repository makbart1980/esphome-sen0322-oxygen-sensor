#include "sen0322.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace sen0322 {

static const char *const TAG = "sen0322";

// Register commands according to datasheet
static const uint8_t SEN0322_COLLECT_PHASE = 0x01;
static const uint8_t SEN0322_JUDGE_PHASE = 0x02;
static const uint8_t SEN0322_OXYGEN_DATA = 0x03;
static const uint8_t SEN0322_GET_KEY_REGISTER = 0x0A;

void SEN0322Sensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SEN0322...");
  
  // Initialize sensor
  if (!this->write_byte(SEN0322_COLLECT_PHASE, 0x00)) {
    ESP_LOGE(TAG, "Failed to initialize sensor");
    this->mark_failed();
    return;
  }
  readFlash();
  ESP_LOGCONFIG(TAG, "SEN0322 setup complete");
}

void SEN0322Sensor::readFlash() {
  uint8_t value = 0;
  value = this->read_byte(SEN0322_GET_KEY_REGISTER).value_or(0);
  ESP_LOGD(TAG, "Got flash value: 0x%02X", value);
  
  if (value == 0) { calibrated_value_ = 20.9 / 120.0; }
  else { calibrated_value_ = (float)value / 1000.0; }
}

void SEN0322Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "SEN0322 Oxygen Sensor:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Oxygen", this);
  
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with SEN0322 failed!");
  }
}

void SEN0322Sensor::update() {
  ESP_LOGV(TAG, "Updating SEN0322 sensor...");
  readFlash();
  // Send command to read oxygen concentration
  //if (!this->write_byte(SEN0322_OXYGEN_DATA, 0x00)) {
  //  ESP_LOGE(TAG, "Failed to send oxygen data command");
  //  this->status_set_warning();
  //  return;
  //}
  
  // Wait for sensor to process
  //delay(50);
  
  // Read 3 bytes of data
  uint8_t data[3];
  if (!this->read_bytes(SEN0322_OXYGEN_DATA, data, 3)) {
    ESP_LOGE(TAG, "Failed to read oxygen data");
    this->status_set_warning();
    return;
  }

  ESP_LOGD(TAG, "Read 3 bytes: [0] = 0x%02X, [1] = 0x%02X, [2] = 0x%02X", data[0], data[1], data[2]);
  
  // Parse oxygen concentration with little-endian byte order
  // SEN0322 uses reversed byte order: data[1] = high byte, data[0] = low byte
  // uint16_t raw_oxygen = (data[1] << 8) | data[0];
  float oxygen_concentration = (calibrated_value_ * (((float)data[0]) + ((float)data[1] / 10.0) + ((float)data[2] / 100.0)));
  
  // Validate reading (oxygen should be between 0-30% for safety margin)
  if (oxygen_concentration < 0.0f || oxygen_concentration > 30.0f) {
    ESP_LOGW(TAG, "Invalid oxygen reading: %.2f%%", oxygen_concentration);
    this->status_set_warning();
    return;
  }
  
  ESP_LOGD(TAG, "Got oxygen concentration: %.2f%%", oxygen_concentration);
  this->publish_state(oxygen_concentration);
  this->status_clear_warning();
}

float SEN0322Sensor::get_setup_priority() const {
  return setup_priority::DATA;
}

}  // namespace sen0322
}  // namespace esphome
