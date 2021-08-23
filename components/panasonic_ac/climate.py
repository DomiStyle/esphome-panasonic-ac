from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, climate, sensor, select, switch

AUTO_LOAD = ["switch", "sensor", "select"]
DEPENDENCIES = ["uart"]

panasonic_ac_ns = cg.esphome_ns.namespace("panasonic_ac")
PanasonicAC = panasonic_ac_ns.class_(
    "PanasonicAC", cg.Component, uart.UARTDevice, climate.Climate
)
panasonic_ac_cnt_ns = panasonic_ac_ns.namespace("CNT")
PanasonicACCNT = panasonic_ac_cnt_ns.class_("PanasonicACCNT", PanasonicAC)
panasonic_ac_wlan_ns = panasonic_ac_ns.namespace("WLAN")
PanasonicACWLAN = panasonic_ac_wlan_ns.class_("PanasonicACWLAN", PanasonicAC)

PanasonicACSwitch = panasonic_ac_ns.class_(
    "PanasonicACSwitch", switch.Switch, cg.Component
)
PanasonicACSelect = panasonic_ac_ns.class_(
    "PanasonicACSelect", select.Select, cg.Component
)


CONF_HORIZONTAL_SWING_SELECT = "horizontal_swing_select"
CONF_VERTICAL_SWING_SELECT = "vertical_swing_select"
CONF_OUTSIDE_TEMPERATURE = "outside_temperature"
CONF_CURRENT_TEMPERATURE_SENSOR = "current_temperature_sensor"
CONF_NANOEX_SWITCH = "nanoex_switch"
CONF_ECO_SWITCH = "eco_switch"
CONF_ECONAVI_SWITCH = "econavi_switch"
CONF_MILD_DRY_SWITCH = "mild_dry_switch"
CONF_WLAN = "wlan"
CONF_CNT = "cnt"

HORIZONTAL_SWING_OPTIONS = [
    "auto",
    "left",
    "left_center",
    "center",
    "right_center",
    "right",
]


VERTICAL_SWING_OPTIONS = ["auto", "up", "up_center", "center", "down_center", "down"]

SWITCH_SCHEMA = switch.SWITCH_SCHEMA.extend(cv.COMPONENT_SCHEMA).extend(
    {cv.GenerateID(): cv.declare_id(PanasonicACSwitch)}
)
SELECT_SCHEMA = select.SELECT_SCHEMA.extend(
    {cv.GenerateID(CONF_ID): cv.declare_id(PanasonicACSelect)}
)

SCHEMA = climate.CLIMATE_SCHEMA.extend(
    {
        cv.Optional(CONF_HORIZONTAL_SWING_SELECT): SELECT_SCHEMA,
        cv.Optional(CONF_VERTICAL_SWING_SELECT): SELECT_SCHEMA,
        cv.Optional(CONF_OUTSIDE_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_NANOEX_SWITCH): SWITCH_SCHEMA,
    }
).extend(uart.UART_DEVICE_SCHEMA)

CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_WLAN: SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(PanasonicACWLAN),
            }
        ),
        CONF_CNT: SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(PanasonicACCNT),
                cv.Optional(CONF_ECO_SWITCH): SWITCH_SCHEMA,
                cv.Optional(CONF_ECONAVI_SWITCH): SWITCH_SCHEMA,
                cv.Optional(CONF_MILD_DRY_SWITCH): SWITCH_SCHEMA,
                cv.Optional(CONF_CURRENT_TEMPERATURE_SENSOR): cv.use_id(sensor.Sensor),
            }
        ),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await climate.register_climate(var, config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CONF_HORIZONTAL_SWING_SELECT in config:
        conf = config[CONF_HORIZONTAL_SWING_SELECT]
        swing_select = await select.new_select(conf, options=HORIZONTAL_SWING_OPTIONS)
        await cg.register_component(swing_select, conf)
        cg.add(var.set_horizontal_swing_select(swing_select))

    if CONF_VERTICAL_SWING_SELECT in config:
        conf = config[CONF_VERTICAL_SWING_SELECT]
        swing_select = await select.new_select(conf, options=VERTICAL_SWING_OPTIONS)
        await cg.register_component(swing_select, conf)
        cg.add(var.set_vertical_swing_select(swing_select))

    if CONF_OUTSIDE_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_OUTSIDE_TEMPERATURE])
        cg.add(var.set_outside_temperature_sensor(sens))

    for s in [CONF_ECO_SWITCH, CONF_NANOEX_SWITCH, CONF_MILD_DRY_SWITCH, CONF_ECONAVI_SWITCH]:
        if s in config:
            conf = config[s]
            a_switch = cg.new_Pvariable(conf[CONF_ID])
            await cg.register_component(a_switch, conf)
            await switch.register_switch(a_switch, conf)
            cg.add(getattr(var, f"set_{s}")(a_switch))

    if CONF_CURRENT_TEMPERATURE_SENSOR in config:
        sens = await cg.get_variable(config[CONF_CURRENT_TEMPERATURE_SENSOR])
        cg.add(var.set_current_temperature_sensor(sens))
