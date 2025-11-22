import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, touchscreen
from esphome.const import CONF_ID
from .. import m5cores3_ns

M5CoreS3Touchscreen = m5cores3_ns.class_(
    "M5CoreS3Touchscreen",
    touchscreen.Touchscreen,
    cg.Component,
)

CONF_LEFT = "left"
CONF_CENTER = "center"
CONF_RIGHT = "right"

CONFIG_SCHEMA = touchscreen.TOUCHSCREEN_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(M5CoreS3Touchscreen),
        cv.Optional(CONF_LEFT): binary_sensor.binary_sensor_schema(),
        cv.Optional(CONF_CENTER): binary_sensor.binary_sensor_schema(),
        cv.Optional(CONF_RIGHT): binary_sensor.binary_sensor_schema(),
    }
)

async def to_code(config):
    touchscreen_ = cg.new_Pvariable(config[CONF_ID])
    await touchscreen.register_touchscreen(touchscreen_, config)
    if CONF_LEFT in config:
        cg.add(
            touchscreen_.set_left(await binary_sensor.new_binary_sensor(config[CONF_LEFT]))
        )
    if CONF_CENTER in config:
        cg.add(
            touchscreen_.set_center(await binary_sensor.new_binary_sensor(config[CONF_CENTER]))
        )
    if CONF_RIGHT in config:
        cg.add(
            touchscreen_.set_right(await binary_sensor.new_binary_sensor(config[CONF_RIGHT]))
        )


