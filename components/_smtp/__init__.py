import pathlib

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_ID, CONF_PASSWORD, CONF_PORT, CONF_USERNAME
from esphome.core import CORE

DEPENDENCIES = ["network"]
CODEOWNERS = ["@rtyle"]

CONF_SERVER = "server"
CONF_FROM = "from"
CONF_TO = "to"
CONF_STARTTLS = "starttls"
CONF_CAS = "cas"
CONF_SUBJECT = "subject"
CONF_BODY = "body"
CONF_TASK_NAME = "task_name"
CONF_TASK_PRIORITY = "task_priority"

_smtp_ns = cg.esphome_ns.namespace("_smtp")
Component = _smtp_ns.class_("Component", cg.Component)
Action = _smtp_ns.class_("Action", automation.Action)


def requires_esp_idf(value):
    if not CORE.using_esp_idf:
        raise cv.Invalid("requires esp32.framework.type: esp-idf)")
    return value


def string_from_file_or_value(value: object) -> str:
    value = cv.string(value)  # value must be a string
    try:
        path: pathlib.Path = cv.file_(value)
    except (cv.Invalid, OSError):
        return value  # value is not a path to a config file, just a string value
    try:
        with open(path, "r", encoding="utf-8") as file:
            return file.read()
    except Exception as e:
        raise cv.Invalid(f"file '{value}' found but could not be read: {e}") from e


MULTI_CONF = True

CONFIG_SCHEMA = cv.All(
    requires_esp_idf,
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Component),
            cv.Required(CONF_SERVER): cv.string,
            cv.Optional(CONF_PORT, default=587): cv.port,
            cv.Required(CONF_USERNAME): cv.string,
            cv.Required(CONF_PASSWORD): cv.string,
            cv.Required(CONF_FROM): cv.string,
            cv.Required(CONF_TO): cv.string,
            cv.Optional(CONF_STARTTLS, default=True): cv.boolean,
            cv.Optional(CONF_CAS): string_from_file_or_value,
            cv.Optional(CONF_TASK_NAME, default="smtp"): cv.string,
            cv.Optional(CONF_TASK_PRIORITY, default=5): cv.int_range(min=0),
        }
    ).extend(cv.COMPONENT_SCHEMA),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_server(config[CONF_SERVER]))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_username(config[CONF_USERNAME]))
    cg.add(var.set_password(config[CONF_PASSWORD]))
    cg.add(var.set_from(config[CONF_FROM]))
    cg.add(var.set_to(config[CONF_TO]))
    cg.add(var.set_starttls(config[CONF_STARTTLS]))
    cg.add(var.set_task_name(config[CONF_TASK_NAME]))
    cg.add(var.set_task_priority(config[CONF_TASK_PRIORITY]))

    if CONF_CAS in config:
        cg.add(var.set_cas(config[CONF_CAS]))


@automation.register_action(
    "_smtp.send",
    Action,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(Component),
            cv.Required(CONF_SUBJECT): cv.templatable(cv.string),
            cv.Optional(CONF_BODY): cv.templatable(cv.string),
            cv.Optional(CONF_TO): cv.templatable(cv.string),
        }
    ),
)
async def smtp_send_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    action = cg.new_Pvariable(action_id, template_arg, parent)

    template_ = await cg.templatable(config[CONF_SUBJECT], args, cg.std_string)
    cg.add(action.set_subject(template_))

    if CONF_BODY in config:
        template_ = await cg.templatable(config[CONF_BODY], args, cg.std_string)
        cg.add(action.set_body(template_))

    if CONF_TO in config:
        template_ = await cg.templatable(config[CONF_TO], args, cg.std_string)
        cg.add(action.set_to(template_))

    return action
