import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import STATE_CLASS_MEASUREMENT, UNIT_PERCENT

from . import sen0322_ns

DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@makbart1980"]

SEN0322Sensor = sen0322_ns.class_(
    "SEN0322Sensor",
    sensor.Sensor,
    cg.PollingComponent,
    i2c.I2CDevice,
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        SEN0322Sensor,
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x73))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
