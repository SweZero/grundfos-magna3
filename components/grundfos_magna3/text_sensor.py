import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from . import Magna3

CONFIG_SCHEMA = text_sensor.text_sensor_schema().extend({
    cv.Required("grundfos_magna3_id"): cv.use_id(Magna3),
})

async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    parent = await cg.get_variable(config["grundfos_magna3_id"])
    cg.add(parent.set_operating_mode_text(var))
