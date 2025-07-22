import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_EMPTY,
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
)

DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@makbart1980", "@SergeyZh"]  # Zmień na swoją nazwę GitHub

sen0322_ns = cg.esphome_ns.namespace("sen0322")
SEN0322Sensor = sen0322_ns.class_("SEN0322Sensor", cg.PollingComponent, i2c.I2CDevice)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        SEN0322Sensor,
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_EMPTY,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(): cv.declare_id(SEN0322Sensor),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x73))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
