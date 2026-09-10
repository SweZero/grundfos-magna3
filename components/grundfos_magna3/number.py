import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import CONF_ID, CONF_UNIT_OF_MEASUREMENT
from . import ns, Magna3

SetpointNumber = ns.class_("SetpointNumber", number.Number)
MODES = {
    "constant_pressure": (0x00, "m", 1.02, 7.45, 0.01),
    "proportional_pressure": (0x01, "m", 1.94, 7.45, 0.01),
    "constant_curve": (0x02, "%", 35, 100, 1),
    "constant_temperature": (0x06, "°C", -10, 130, 1),
    "constant_flow": (0x08, "m³/h", 1.2, 10.5, 0.1),
    "differential_temperature": (0x0C, "°C", 0, 140, 1),
}

def validate_unit(config):
    unit = MODES[config["control_mode"]][1]
    if CONF_UNIT_OF_MEASUREMENT in config and config[CONF_UNIT_OF_MEASUREMENT] != unit:
        raise cv.Invalid(f"This setpoint requires unit {unit}")
    config[CONF_UNIT_OF_MEASUREMENT] = unit
    return config

CONFIG_SCHEMA = cv.All(number.number_schema(SetpointNumber, icon="mdi:tune").extend({
    cv.Required("grundfos_magna3_id"): cv.use_id(Magna3),
    cv.Required("control_mode"): cv.one_of(*MODES, lower=True),
}), validate_unit)

async def to_code(config):
    raw, unit, minimum, maximum, step = MODES[config["control_mode"]]
    var = cg.new_Pvariable(config[CONF_ID])
    await number.register_number(var, config, min_value=minimum, max_value=maximum, step=step)
    parent = await cg.get_variable(config["grundfos_magna3_id"])
    cg.add(var.set_parent(parent))
    cg.add(var.set_control_mode(raw))
    cg.add(parent.set_number(raw, var))
