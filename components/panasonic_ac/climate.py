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

CONF_SUPPORTED = "supported"
CONF_SELECTOR = "selector"
CONF_SWITCH = "switch"

CONF_TRAITS = "traits"
CONF_HORIZONTAL_SWING_MODE = "horizontal_swing_mode"
CONF_VERTICAL_SWING_MODE = "vertical_swing_mode"
CONF_NANOEX_MODE = "nanoex_mode"

CONF_SENSORS = "sensors"
CONF_SENSOR = "sensor"
CONF_OUTSIDE_TEMPERATURE = "outside_temperature"
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

def _validate_selector_schema(config):
    if cv.boolean(config[CONF_SUPPORTED]) and CONF_SELECTOR not in config:
        raise cv.Invalid(f"'{CONF_SELECTOR}' is required if '{CONF_SUPPORTED}' is set to {config[CONF_SUPPORTED]}!")
    return config

HORIZONTAL_SWING_MODE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_SUPPORTED): cv.boolean,
        cv.Optional(CONF_SELECTOR): SELECT_SCHEMA
    }
).add_extra(_validate_selector_schema)

VERTICAL_SWING_MODE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_SUPPORTED): cv.boolean,
        cv.Optional(CONF_SELECTOR): SELECT_SCHEMA
    }
).add_extra(_validate_selector_schema)

def _validate_switch_schema(config):
    if cv.boolean(config[CONF_SUPPORTED]) and CONF_SWITCH not in config:
        raise cv.Invalid(f"'{CONF_SWITCH}' is required if '{CONF_SUPPORTED}' is set to {config[CONF_SUPPORTED]}!")
    return config

NANOEX_MODE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_SUPPORTED): cv.boolean,
        cv.Optional(CONF_SWITCH): SWITCH_SCHEMA
    }
).add_extra(_validate_switch_schema)

TRAITS_SCHEMA = cv.Schema({
    cv.Required(CONF_VERTICAL_SWING_MODE): VERTICAL_SWING_MODE_SCHEMA,
    cv.Required(CONF_HORIZONTAL_SWING_MODE): HORIZONTAL_SWING_MODE_SCHEMA,
    cv.Required(CONF_NANOEX_MODE): NANOEX_MODE_SCHEMA
})

def _validate_sensor_schema(config):
    if cv.boolean(config[CONF_SUPPORTED]) and CONF_SENSOR not in config:
        raise cv.Invalid(f"'{CONF_SENSOR}' is required if '{CONF_SUPPORTED}' is set to {config[CONF_SUPPORTED]}!")
    return config

OUTSIDE_TEMPERATURE_SENSOR_SCHEMA = cv.Schema({
    cv.Required(CONF_SUPPORTED): cv.boolean,
    cv.Optional(CONF_SENSOR): sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT
    )
}).add_extra(_validate_sensor_schema)

CONF_CURRENT_POWER_CONSUMPTION_SCHEMA = cv.Schema({
    cv.Required(CONF_SUPPORTED): cv.boolean,
    cv.Optional(CONF_SENSOR): sensor.sensor_schema(
        unit_of_measurement=UNIT_WATT,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT
    )
}).add_extra(_validate_sensor_schema)

SENSORS_SCHEMA = cv.Schema({
    cv.Required(CONF_OUTSIDE_TEMPERATURE): OUTSIDE_TEMPERATURE_SENSOR_SCHEMA,
    cv.Required(CONF_CURRENT_POWER_CONSUMPTION): CONF_CURRENT_POWER_CONSUMPTION_SCHEMA
})

CONFIG_SCHEMA = climate.CLIMATE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(PanasonicACCNT),
        cv.Required(CONF_TRAITS): TRAITS_SCHEMA,
        cv.Required(CONF_SENSORS): SENSORS_SCHEMA
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

async def setup_sensors_(panasonic_ac, sensors_config):
    if CONF_OUTSIDE_TEMPERATURE in sensors_config:
        outside_temp_config = sensors_config[CONF_OUTSIDE_TEMPERATURE]
        if cv.boolean(outside_temp_config[CONF_SUPPORTED]):
            outside_temp_sensor = await sensor.new_sensor(outside_temp_config[CONF_SENSOR])
            cg.add(panasonic_ac.set_outside_temperature_sensor(outside_temp_sensor))

    if CONF_CURRENT_POWER_CONSUMPTION in sensors_config:
        consumption_config = sensors_config[CONF_CURRENT_POWER_CONSUMPTION]
        if cv.boolean(consumption_config[CONF_SUPPORTED]):
            consumption_sensor = await sensor.new_sensor(consumption_config[CONF_SENSOR])
            cg.add(panasonic_ac.set_current_power_consumption_sensor(consumption_sensor))

async def to_code(config):
    panasonic_ac = cg.new_Pvariable(config[CONF_ID])
    
    await climate.register_climate(panasonic_ac, config)
    await cg.register_component(panasonic_ac, config)
    await uart.register_uart_device(panasonic_ac, config)
    
    setup_default_icon_(panasonic_ac, config, "mdi:air-conditioner")
    
    if CONF_TRAITS in config:
        await setup_traits_(panasonic_ac, config[CONF_TRAITS])

    if CONF_SENSORS in config:
        await setup_sensors_(panasonic_ac, config[CONF_SENSORS])
