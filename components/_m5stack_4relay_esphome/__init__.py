import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components import switch
from esphome.components import i2c

CODEOWNERS = ["@rtyle"]

DEPENDENCIES = ["i2c"]

m5stack_4relay_esphome_ns = cg.esphome_ns.namespace("_m5stack_4relay_esphome")

Interface = m5stack_4relay_esphome_ns.class_("Interface", cg.Component, i2c.I2CDevice)
Relay = m5stack_4relay_esphome_ns.class_("Relay", switch.Switch)

CONF_RELAYS = "relays"

CONF_INDEX = "index"

CONFIG_SCHEMA = cv.All(
    cv.ensure_list(
        cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(Interface),
                cv.Optional(CONF_RELAYS): cv.ensure_list(
                    switch.switch_schema(Relay).extend(
                        {
                            cv.Optional(CONF_INDEX, default=0): cv.int_range(
                                min=0, max=3
                            ),
                        }
                    )
                ),
            }
        )
        .extend(cv.COMPONENT_SCHEMA)
        .extend(i2c.i2c_device_schema(default_address=0x26))
    )
)


async def to_code(configs):
    for config in configs:
        interface = cg.new_Pvariable(config[CONF_ID])
        await cg.register_component(interface, config)
        await i2c.register_i2c_device(interface, config)
        if CONF_RELAYS in config:
            for relay_config in config[CONF_RELAYS]:
                relay = await switch.new_switch(relay_config)
                cg.add(relay.set_interface(interface))
                cg.add(relay.set_index(relay_config[CONF_INDEX]))
