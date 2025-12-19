import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components.esp32 import CONF_SDKCONFIG_OPTIONS, add_idf_component
from esphome.const import CONF_FRAMEWORK, Platform
from esphome.core import CORE

CONF_SSL_SUPPORT = "ssl_support"

CONFIG_LWIP_IPV6 = "CONFIG_LWIP_IPV6"
CONFIG_ASIO_IS_ENABLED = "CONFIG_ASIO_IS_ENABLED"
CONFIG_ASIO_SSL_SUPPORT = "CONFIG_ASIO_SSL_SUPPORT"


def requires_esp_idf(value):
    if not CORE.using_esp_idf:
        raise cv.Invalid("requires esp32.framework.type: esp-idf)")
    return value


CONFIG_SCHEMA = cv.All(
    requires_esp_idf,
    cv.Schema(
        {
            cv.Optional(CONF_SSL_SUPPORT, default="no"): cv.boolean,
        }
    ),
)


def _blaze(parent: dict, path: list, default=None):
    for child in path:
        if isinstance(parent, dict):
            if child in parent:
                parent = parent[child]
            else:
                parent = parent[child] = {}
        else:
            return default
    return parent


def _final_validate(config):
    sdkconfig_options = _blaze(
        fv.full_config.get(), [Platform.ESP32, CONF_FRAMEWORK, CONF_SDKCONFIG_OPTIONS]
    )
    if sdkconfig_options is not None:
        sdkconfig_options[CONFIG_LWIP_IPV6] = "y"
        sdkconfig_options[CONFIG_ASIO_IS_ENABLED] = "y"
        if config.get(CONF_SSL_SUPPORT, False):
            sdkconfig_options[CONFIG_ASIO_SSL_SUPPORT] = "y"


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    add_idf_component(
        name="espressif/asio",
        repo="https://github.com/rtyle/esp-protocols.git",
        path="components/asio",
        ref="fix/asio/memory-leaks-over-ssl-stream-lifetime"
    )
