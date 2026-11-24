import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, climate, select
from esphome.const import CONF_ID, CONF_NAME, CONF_UART_ID

CODEOWNERS = ["Pablo"]
DEPENDENCIES  = ["climate", "uart"]
AUTO_LOAD = ["sensor", "select"]

rotenso_ns = cg.esphome_ns.namespace('rotenso')
RotensoClimate = rotenso_ns.class_('RotensoClimate', climate.Climate, cg.Component, uart.UARTDevice)
RotensoSwingSelect = rotenso_ns.class_('RotensoSwingSelect', select.Select)

CONF_VERTICAL_SWING_SELECT = "vertical_swing_select"
CONF_HORIZONTAL_SWING_SELECT = "horizontal_swing_select"

POSITION_OPTIONS = ["Stopped"] + [f"Position {i}" for i in range(1, 6)]
VERTICAL_OPTIONS = POSITION_OPTIONS + ["Swing 1-3", "Swing 3-5", "Swing 1-5", "Unknown"]
HORIZONTAL_OPTIONS = POSITION_OPTIONS + ["Swing 1-5", "Swing 1-3", "Swing 2-4", "Swing 3-5", "Unknown"]


def add_swing_select_defaults(config):
    if not isinstance(config, dict):
        return config
    name = config.get(CONF_NAME) or str(config.get(CONF_ID) or "Rotenso")
    config.setdefault(CONF_VERTICAL_SWING_SELECT, {"name": f"{name} Vertical vane"})
    config.setdefault(CONF_HORIZONTAL_SWING_SELECT, {"name": f"{name} Horizontal vane"})
    return config


CONFIG_SCHEMA = cv.All(
    add_swing_select_defaults,
    climate.climate_schema(RotensoClimate)
    .extend(
        {
            cv.GenerateID(CONF_UART_ID): cv.use_id(uart.UARTComponent),
            cv.Optional(CONF_VERTICAL_SWING_SELECT): select.select_schema(RotensoSwingSelect),
            cv.Optional(CONF_HORIZONTAL_SWING_SELECT): select.select_schema(RotensoSwingSelect),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    await climate.register_climate(var, config)
    vertical = await select.new_select(config[CONF_VERTICAL_SWING_SELECT], options=VERTICAL_OPTIONS)
    horizontal = await select.new_select(config[CONF_HORIZONTAL_SWING_SELECT], options=HORIZONTAL_OPTIONS)
    cg.add(var.set_vertical_swing_select(vertical))
    cg.add(var.set_horizontal_swing_select(horizontal))

