import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import CONF_ID
from . import ns, Magna3

ControlModeSelect = ns.class_("ControlModeSelect", select.Select)
OPTIONS = ["Constant Pressure", "Proportional Pressure", "Constant Curve", "AUTOADAPT",
           "Constant Temperature", "Constant Flow", "FLOWADAPT", "Differential Temperature"]
CONFIG_SCHEMA = select.select_schema(ControlModeSelect).extend({
    cv.Required("grundfos_magna3_id"): cv.use_id(Magna3),
})

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await select.register_select(var, config, options=OPTIONS)
    parent = await cg.get_variable(config["grundfos_magna3_id"])
    cg.add(var.set_parent(parent))
    cg.add(parent.set_mode_select(var))
