import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.cpp_generator import MockObjClass

DEPENDENCIES = ["sensor"]

since_ns = cg.esphome_ns.namespace("_since")
Since = since_ns.class_("Since", cg.PollingComponent, sensor.Sensor)

CONF_WHEN = "when"


def since_schema(class_: MockObjClass = Since) -> cv.Schema:
    return (
        cv.Schema(
            {
                cv.Optional(CONF_WHEN): cv.positive_time_period_nanoseconds,
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


CONFIG_SCHEMA = cv.All(cv.ensure_list(since_schema()))


async def to_code(configs):
    for config in configs:
        since = await sensor.new_sensor(config)
        await cg.register_component(since, config)
        if CONF_WHEN in config:
            cg.add(since.set_when(config[CONF_WHEN]))
