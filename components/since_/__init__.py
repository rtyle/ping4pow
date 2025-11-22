import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, text_sensor
from esphome.cpp_generator import MockObjClass

DEPENDENCIES = ["sensor", "text_sensor", "format_"]

since_ns = cg.esphome_ns.namespace("since_")
Since = since_ns.class_("Since", cg.PollingComponent, sensor.Sensor)

CONF_WHEN = "when"
CONF_TEXT = "text"
CONF_LABEL = "label"


def since_schema(class_: MockObjClass = Since) -> cv.Schema:
    return (
        cv.Schema(
            {
                cv.Optional(CONF_WHEN): cv.positive_time_period_nanoseconds,
                cv.Optional(CONF_LABEL): cv.use_id(cg.global_ns.class_("lv_obj_t")),
                cv.Optional(CONF_TEXT): text_sensor.text_sensor_schema(),
            }
        )
        .extend(
            sensor.sensor_schema(
                class_,
                unit_of_measurement="s",
                accuracy_decimals=0,
                device_class="duration",
                state_class="measurement",
            )
        )
        .extend(cv.polling_component_schema("10s"))
    )


MULTI_CONF = True

CONFIG_SCHEMA = since_schema()


async def to_code(config):
    since = await sensor.new_sensor(config)
    await cg.register_component(since, config)
    if CONF_WHEN in config:
        cg.add(since.set_when(config[CONF_WHEN]))
    if CONF_LABEL in config:
        cg.add(since.set_label(await cg.get_variable(config[CONF_LABEL])))
    if CONF_TEXT in config:
        cg.add(since.set_text(await text_sensor.new_text_sensor(config[CONF_TEXT])))
