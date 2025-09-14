import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.core
from esphome.const import CONF_ID

DEPENDENCIES = []
CODEOWNERS = ["@rtyle"]

rotation_ns = cg.esphome_ns.namespace("_rotation")
Rotation = rotation_ns.class_("Rotation<void *>", cg.Component)
RotationIterator = rotation_ns.class_("Rotation<void *>::Iterator")

CONF_ITEMS = "items"
CONF_ITERATORS = "iterators"

CONFIG_SCHEMA = cv.All(
    cv.ensure_list(
        cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(Rotation),
                cv.Required(CONF_ITEMS): cv.All(cv.ensure_list(cv.string)),
                cv.Required(CONF_ITERATORS): cv.All(
                    cv.ensure_list(
                        cv.Schema(
                            {
                                cv.Required(CONF_ID): cv.declare_id(RotationIterator),
                            }
                        )
                    ),
                ),
            }
        ).extend(cv.COMPONENT_SCHEMA)
    )
)


async def to_code(configs):
    for config in configs:
        rotation = cg.new_Pvariable(config[CONF_ID])
        await cg.register_component(rotation, config)
        for item in config[CONF_ITEMS]:
            cg.add(rotation.add(await cg.get_variable(esphome.core.ID(item))))
        for iterator in config[CONF_ITERATORS]:
            cg.new_Pvariable(iterator[CONF_ID], rotation)
