"""Minimal Grundfos MAGNA3 GENI/BLE setpoint component."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client
from esphome.const import CONF_ID

DEPENDENCIES = ["ble_client"]
AUTO_LOAD = ["number", "sensor", "select", "switch", "text_sensor"]
MULTI_CONF = True
ns = cg.esphome_ns.namespace("grundfos_magna3")
Magna3 = ns.class_("Magna3", cg.PollingComponent, ble_client.BLEClientNode)
CONFIG_SCHEMA = cv.Schema({cv.GenerateID(): cv.declare_id(Magna3), cv.Optional("request_encryption", default=False): cv.boolean}).extend(
    ble_client.BLE_CLIENT_SCHEMA
).extend(cv.polling_component_schema("30s"))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_request_encryption(config["request_encryption"]))
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)
