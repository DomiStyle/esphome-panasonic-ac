from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_ICON,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_POWER,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_WATT,
)
from esphome import core
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

PanasonicACSwitch = panasonic_ac_ns.class_(
    "PanasonicACSwitch", switch.Switch, cg.Component
)
PanasonicACSelect = panasonic_ac_ns.class_(
    "PanasonicACSelect", select.Select, cg.Component
)

CONF_TRAITS = "traits"
CONF_HORIZONTAL_SWING_MODE = "horizontal_swing_mode"
CONF_VERTICAL_SWING_MODE = "vertical_swing_mode"
CONF_NANOEX_MODE = "nanoex_mode"
CONF_SELECTOR = "selector"
CONF_SWITCH = "switch"
CONF_SUPPORTED = "supported"

CONF_OUTSIDE_TEMPERATURE = "outside_temperature"
CONF_CURRENT_TEMPERATURE_SENSOR = "current_temperature_sensor"
CONF_ECO_SWITCH = "eco_switch"
CONF_ECONAVI_SWITCH = "econavi_switch"
CONF_MILD_DRY_SWITCH = "mild_dry_switch"
CONF_CURRENT_POWER_CONSUMPTION = "current_power_consumption"

HORIZONTAL_SWING_OPTIONS = [
    "auto",
    "left",
    "left_center",
    "center",
    "right_center",
    "right"
]


VERTICAL_SWING_OPTIONS = [
    "swing",
    "auto",
    "up",
    "up_center",
    "center",
    "down_center",
    "down"
]

SWITCH_SCHEMA = switch.SWITCH_SCHEMA.extend(cv.COMPONENT_SCHEMA).extend(
    {cv.GenerateID(): cv.declare_id(PanasonicACSwitch)}
)
SELECT_SCHEMA = select.SELECT_SCHEMA.extend(
    {cv.GenerateID(CONF_ID): cv.declare_id(PanasonicACSelect)}
)

HORIZONTAL_SWING_MODE_SCHEMA = cv.Schema({
    cv.Required(CONF_SUPPORTED): cv.boolean,
    cv.Optional(CONF_SELECTOR): SELECT_SCHEMA
})

VERTICAL_SWING_MODE_SCHEMA = cv.Schema({
    cv.Required(CONF_SUPPORTED): cv.boolean,
    cv.Optional(CONF_SELECTOR): SELECT_SCHEMA
})

NANOEX_MODE_SCHEMA = cv.Schema({
    cv.Required(CONF_SUPPORTED): cv.boolean,
    cv.Optional(CONF_SWITCH): SWITCH_SCHEMA
})

TRAITS_SCHEMA = cv.Schema({
    cv.Optional(CONF_VERTICAL_SWING_MODE): VERTICAL_SWING_MODE_SCHEMA,
    cv.Optional(CONF_HORIZONTAL_SWING_MODE): HORIZONTAL_SWING_MODE_SCHEMA,
    cv.Optional(CONF_NANOEX_MODE): NANOEX_MODE_SCHEMA
})

CONFIG_SCHEMA = climate.CLIMATE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(PanasonicACCNT),
        cv.Optional(CONF_TRAITS): TRAITS_SCHEMA,
        cv.Optional(CONF_OUTSIDE_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_CURRENT_TEMPERATURE_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_CURRENT_POWER_CONSUMPTION): sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT
        )
    }
).extend(uart.UART_DEVICE_SCHEMA)

def setup_default_icon_(var, config, default_icon):
    if CONF_ICON not in config:
        cg.add(var.set_icon(default_icon))

async def setup_swing_mode_(swing_mode_config, swing_selector_default_icon, swing_selector_options):
    if cv.boolean(swing_mode_config[CONF_SUPPORTED]):
        swing_selector_config = swing_mode_config[CONF_SELECTOR]
        swing_selector = await select.new_select(swing_selector_config, options=swing_selector_options)
        await cg.register_component(swing_selector, swing_selector_config)
        setup_default_icon_(swing_selector, swing_selector_config, swing_selector_default_icon)
        return swing_selector
    else:
        return None

async def setup_traits_(panasonic_ac, traits_config):
        if CONF_HORIZONTAL_SWING_MODE in traits_config:
            horizontal_swing_mode_config = traits_config[CONF_HORIZONTAL_SWING_MODE]
            horizontal_swing_selector = await setup_swing_mode_(horizontal_swing_mode_config, "mdi:arrow-left-right", HORIZONTAL_SWING_OPTIONS)
            if horizontal_swing_selector is not None:
                cg.add(panasonic_ac.set_horizontal_swing_select(horizontal_swing_selector))
        
        if CONF_VERTICAL_SWING_MODE in traits_config:
            vertical_swing_mode_config = traits_config[CONF_VERTICAL_SWING_MODE]
            vertical_swing_selector = await setup_swing_mode_(vertical_swing_mode_config, "mdi:arrow-up-down", VERTICAL_SWING_OPTIONS)
            if vertical_swing_selector is not None:
                cg.add(panasonic_ac.set_vertical_swing_select(vertical_swing_selector))

        if CONF_NANOEX_MODE in traits_config:
            nanoex_mode_config = traits_config[CONF_NANOEX_MODE]
            if cv.boolean(nanoex_mode_config[CONF_SUPPORTED]):
                nanoex_switch_config = nanoex_mode_config[CONF_SWITCH]
                nanoex_switch = await switch.new_switch(nanoex_switch_config)
                await cg.register_component(nanoex_switch, nanoex_switch_config)
                setup_default_icon_(nanoex_switch, nanoex_switch_config, "mdi:air-filter")
                cg.add(panasonic_ac.set_nanoex_switch(nanoex_switch))


async def to_code(config):
    panasonic_ac = cg.new_Pvariable(config[CONF_ID])
    
    await climate.register_climate(panasonic_ac, config)
    await cg.register_component(panasonic_ac, config)
    await uart.register_uart_device(panasonic_ac, config)
    
    setup_default_icon_(panasonic_ac, config, "mdi:air-conditioner")
    
    if CONF_TRAITS in config:
        await setup_traits_(panasonic_ac, config[CONF_TRAITS])

    # legacy codes, should be deleted
    if CONF_OUTSIDE_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_OUTSIDE_TEMPERATURE])
        cg.add(panasonic_ac.set_outside_temperature_sensor(sens))

    for s in [CONF_ECO_SWITCH, CONF_MILD_DRY_SWITCH, CONF_ECONAVI_SWITCH]:
        if s in config:
            conf = config[s]
            a_switch = cg.new_Pvariable(conf[CONF_ID])
            await cg.register_component(a_switch, conf)
            await switch.register_switch(a_switch, conf)
            cg.add(getattr(panasonic_ac, f"set_{s}")(a_switch))

    if CONF_CURRENT_TEMPERATURE_SENSOR in config:
        sens = await cg.get_variable(config[CONF_CURRENT_TEMPERATURE_SENSOR])
        cg.add(panasonic_ac.set_current_temperature_sensor(sens))

    if CONF_CURRENT_POWER_CONSUMPTION in config:
        sens = await sensor.new_sensor(config[CONF_CURRENT_POWER_CONSUMPTION])
        cg.add(panasonic_ac.set_current_power_consumption_sensor(sens))
