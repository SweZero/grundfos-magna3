"""Class 0x02 bulk telemetry, with derived head and starts appended."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from . import Magna3

# key, unit, accuracy, device class, state class
SENSORS = (
    ("operating_hours", "h", 0, "duration", "total_increasing"),
    ("motor_current", "A", 1, "current", "measurement"),
    ("power", "W", 0, "power", "measurement"),
    ("motor_speed", "rpm", 0, None, "measurement"),
    ("head_raw", None, 0, None, "measurement"),
    ("flow_raw", None, 0, None, "measurement"),
    ("liquid_temperature", "°C", 0, "temperature", "measurement"),
    ("control_source_raw", None, 0, None, None),
    ("energy_consumption", "kWh", 0, "energy", "total_increasing"),
    ("head", "m", 1, None, "measurement"),
    ("number_of_starts", None, 0, None, "total_increasing"),
    ("control_mode_raw", None, 0, None, None),
)

def schema(unit, accuracy, device_class, state_class):
    options = {"accuracy_decimals": accuracy}
    if unit is not None:
        options["unit_of_measurement"] = unit
    if device_class is not None:
        options["device_class"] = device_class
    if state_class is not None:
        options["state_class"] = state_class
    return sensor.sensor_schema(**options)

CONFIG_SCHEMA = cv.Schema({
    cv.Required("grundfos_magna3_id"): cv.use_id(Magna3),
    **{cv.Optional(key): schema(unit, accuracy, device_class, state_class)
       for key, unit, accuracy, device_class, state_class in SENSORS},
})

async def to_code(config):
    parent = await cg.get_variable(config["grundfos_magna3_id"])
    for index, (key, *_) in enumerate(SENSORS):
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(parent.set_telemetry_sensor(index, sens))
