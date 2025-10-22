import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components import switch

CODEOWNERS = ["@rtyle"]

DEPENDENCIES = ["switch"]

m5stack_4relay_lgfx_ns = cg.esphome_ns.namespace("_m5stack_4relay_lgfx")

Interface = m5stack_4relay_lgfx_ns.class_("Interface", cg.Component)
Relay = m5stack_4relay_lgfx_ns.class_("Relay", switch.Switch)

CONF_PORT = "port"
CONF_ADDRESS = "address"
CONF_RELAYS = "relays"

CONF_INDEX = "index"

MULTI_CONF = True

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Interface),
        cv.Optional(CONF_PORT): cv.int_range(min=0, max=1),
        cv.Optional(CONF_ADDRESS): cv.int_range(min=0, max=255),
        cv.Optional(CONF_RELAYS): cv.ensure_list(
            switch.switch_schema(Relay).extend(
                {
                    cv.Optional(CONF_INDEX, default=0): cv.int_range(min=0, max=3),
                }
            )
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    interface = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(interface, config)
    if CONF_PORT in config:
        cg.add(interface.set_port(config[CONF_PORT]))
    if CONF_ADDRESS in config:
        cg.add(interface.set_address(config[CONF_ADDRESS]))
    if CONF_RELAYS in config:
        for relay_config in config[CONF_RELAYS]:
            relay = await switch.new_switch(relay_config)
            cg.add(relay.set_interface(interface))
            cg.add(relay.set_index(relay_config[CONF_INDEX]))
