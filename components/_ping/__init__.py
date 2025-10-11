import socket

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import _since, binary_sensor, sensor, switch
from esphome.const import (CONF_ADDRESS, CONF_ID, CONF_INTERVAL, CONF_NAME,
                           CONF_TIMEOUT)

DEPENDENCIES = ["sensor", "binary_sensor"]

ping_ns = cg.esphome_ns.namespace("_ping")
Ping = ping_ns.class_("Ping", cg.Component)
Target = ping_ns.class_("Target", switch.Switch)

CONF_NONE = "none"
CONF_SOME = "some"
CONF_ALL = "all"
CONF_COUNT = "count"
CONF_TARGETS = "targets"

CONF_ABLE = "able"
CONF_SINCE = "since"


def resolvable(address: str) -> str:
    try:
        return str(cv.ipv4address(address))
    except cv.Invalid:
        try:
            return str(cv.ipv4address(socket.gethostbyname(address)))
        except socket.gaierror as e:
            raise cv.Invalid(f"{address} not resolved: {e}")


CONFIG_SCHEMA = cv.All(
    cv.ensure_list(
        cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(Ping),
                cv.Optional(CONF_NAME): cv.string,
                cv.Optional(CONF_NONE): binary_sensor.binary_sensor_schema(),
                cv.Optional(CONF_SOME): binary_sensor.binary_sensor_schema(),
                cv.Optional(CONF_ALL): binary_sensor.binary_sensor_schema(),
                cv.Optional(CONF_COUNT): sensor.sensor_schema(),
                cv.Optional(CONF_SINCE): _since.since_schema(),
                cv.Optional(CONF_TARGETS): cv.ensure_list(
                    switch.switch_schema(Target).extend(
                        {
                            cv.GenerateID(): cv.declare_id(Target),
                            cv.Required(CONF_ADDRESS): resolvable,
                            cv.Optional(
                                CONF_INTERVAL, default="16s"
                            ): cv.positive_time_period_milliseconds,
                            cv.Optional(
                                CONF_TIMEOUT, default="4s"
                            ): cv.positive_time_period_milliseconds,
                            cv.Optional(
                                CONF_ABLE
                            ): binary_sensor.binary_sensor_schema(),
                            cv.Optional(CONF_SINCE): _since.since_schema(),
                        }
                    )
                ),
            }
        ).extend(cv.COMPONENT_SCHEMA)
    )
)


async def to_code(configs):
    for config in configs:
        ping = cg.new_Pvariable(config[CONF_ID])
        await cg.register_component(ping, config)
        if CONF_NONE in config:
            cg.add(
                ping.set_none(await binary_sensor.new_binary_sensor(config[CONF_NONE]))
            )
        if CONF_SOME in config:
            cg.add(
                ping.set_some(await binary_sensor.new_binary_sensor(config[CONF_SOME]))
            )
        if CONF_ALL in config:
            cg.add(
                ping.set_all(await binary_sensor.new_binary_sensor(config[CONF_ALL]))
            )
        if CONF_COUNT in config:
            cg.add(ping.set_count(await sensor.new_sensor(config[CONF_COUNT])))
        if CONF_SINCE in config:
            since_config = config[CONF_SINCE]
            await _since.to_code([since_config])
            cg.add(ping.set_since(await cg.get_variable(since_config[CONF_ID])))
        if CONF_TARGETS in config:
            for target_config in config[CONF_TARGETS]:
                target = await switch.new_switch(target_config)
                cg.add(target.set_ping(ping))
                cg.add(target.set_address(target_config[CONF_ADDRESS]))
                cg.add(target.set_timeout(target_config[CONF_TIMEOUT]))
                cg.add(target.set_interval(target_config[CONF_INTERVAL]))
                if CONF_ABLE in target_config:
                    cg.add(
                        target.set_able(
                            await binary_sensor.new_binary_sensor(
                                target_config[CONF_ABLE]
                            )
                        )
                    )
                if CONF_SINCE in target_config:
                    since_config = target_config[CONF_SINCE]
                    await _since.to_code([since_config])
                    cg.add(
                        target.set_since(await cg.get_variable(since_config[CONF_ID]))
                    )
