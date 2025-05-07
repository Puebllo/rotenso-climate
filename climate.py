import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import (
    uart, climate, sensor
    )
#from esphome.components.climate import ClimateMode, ClimatePreset, ClimateSwingMode#, CLIMATE_VISUAL_SCHEMA

from esphome.const import (
    CONF_ID, CONF_UART_ID, CONF_NAME, CONF_DISABLED_BY_DEFAULT
    )

CODEOWNERS = ["Pablo"]
DEPENDENCIES  = ["climate", "uart"]
AUTO_LOAD = ["sensor"]

rotenso_ns = cg.esphome_ns.namespace('rotenso')
RotensoClimate = rotenso_ns.class_('RotensoClimate', climate.Climate, cg.Component, uart.UARTDevice)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(RotensoClimate),
    cv.GenerateID(CONF_UART_ID): cv.use_id(uart.UARTComponent),
    cv.Required(CONF_NAME): cv.string,
    cv.Optional(CONF_DISABLED_BY_DEFAULT, default=False): cv.boolean#,
   # cv.Optional(CONF_VISUAL): climate.CLIMATE_VISUAL_SCHEMA
}).extend(cv.COMPONENT_SCHEMA)


CONFIG_SCHEMA = cv.All(
    climate.CLIMATE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(RotensoClimate),
            cv.GenerateID(CONF_UART_ID): cv.use_id(uart.UARTComponent)
        }
    ).extend(cv.COMPONENT_SCHEMA)
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    await climate.register_climate(var, config)

