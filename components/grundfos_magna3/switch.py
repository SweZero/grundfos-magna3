import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID
from . import ns, Magna3

PumpRunningSwitch = ns.class_("PumpRunningSwitch", switch.Switch)
CONFIG_SCHEMA = switch.switch_schema(
    PumpRunningSwitch, icon="mdi:pump", block_inverted=True
).extend({cv.Required("grundfos_magna3_id"): cv.use_id(Magna3)})

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await switch.register_switch(var, config)
    parent = await cg.get_variable(config["grundfos_magna3_id"])
    cg.add(var.set_parent(parent))
    cg.add(parent.set_running_switch(var))
