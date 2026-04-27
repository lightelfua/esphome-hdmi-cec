import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins
from esphome.core import CORE
from esphome.const import CONF_ID, CONF_TRIGGER_ID, CONF_DATA, CONF_SOURCE, CONF_ADDRESS, CONF_PIN, CONF_ON_MESSAGE

CONF_DESTINATION = "destination"
CONF_OPCODE = "opcode"
CONF_PHYSICAL_ADDRESS = "physical_address"
CONF_PROMISCUOUS_MODE = "promiscuous_mode"

hdmi_cec_ns = cg.esphome_ns.namespace("hdmi_cec")
HdmiCecComponent = hdmi_cec_ns.class_("HdmiCec", cg.Component)
HdmiCecTrigger = hdmi_cec_ns.class_(
    "HdmiCecTrigger",
    automation.Trigger.template(cg.uint8, cg.uint8, cg.std_vector.template(cg.uint8)),
    cg.Component,
)

def validate_raw_data(value):
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid("data must be list of bytes")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(HdmiCecComponent),
        cv.Required(CONF_ADDRESS): cv.int_range(0, 15),
        cv.Optional(CONF_PHYSICAL_ADDRESS, default=0x0): cv.int_range(0, 0xFFFE),
        cv.Optional(CONF_PROMISCUOUS_MODE, default=False): cv.boolean,
        cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_ON_MESSAGE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(HdmiCecTrigger),
                cv.Optional(CONF_SOURCE): cv.int_range(0, 15),
                cv.Optional(CONF_DESTINATION): cv.int_range(0, 15),
                cv.Optional(CONF_OPCODE): cv.int_range(0, 255),
                cv.Optional(CONF_DATA): cv.templatable(validate_raw_data),
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)

@automation.register_action(
    "hdmi_cec.send",
    hdmi_cec_ns.class_("HdmiCecSendAction", automation.Action),
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(HdmiCecComponent),
            cv.Optional(CONF_SOURCE): cv.templatable(cv.int_range(0, 15)),
            cv.Required(CONF_DESTINATION): cv.templatable(cv.int_range(0, 15)),
            cv.Required(CONF_DATA): cv.templatable(validate_raw_data),
        }
    ),
)
async def hdmi_cec_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

    if CONF_SOURCE in config:
        src = config[CONF_SOURCE]
        if cg.is_template(src):
            templ = await cg.templatable(src, args, cg.uint8)
            cg.add(var.set_source_template(templ))
        else:
            cg.add(var.set_source(src))

    dst = config[CONF_DESTINATION]
    if cg.is_template(dst):
        templ = await cg.templatable(dst, args, cg.uint8)
        cg.add(var.set_destination_template(templ))
    else:
        cg.add(var.set_destination(dst))

    data = config[CONF_DATA]
    if isinstance(data, bytes):
        data = list(data)

    if cg.is_template(data):
        templ = await cg.templatable(data, args, cg.std_vector.template(cg.uint8))
        cg.add(var.set_data_template(templ))
    else:
        cg.add(var.set_data_static(data))

    return var

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add_library(
        name="hdmi_cec",
        repository="https://github.com/lightelfua/CEC.git",
        version=None,
    )

    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_physical_address(config[CONF_PHYSICAL_ADDRESS]))
    cg.add(var.set_promiscuous_mode(config[CONF_PROMISCUOUS_MODE]))

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))

    for conf in config.get(CONF_ON_MESSAGE, []):
        trig = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)

        if CONF_SOURCE in conf:
            cg.add(trig.set_source(conf[CONF_SOURCE]))
        if CONF_DESTINATION in conf:
            cg.add(trig.set_destination(conf[CONF_DESTINATION]))
        if CONF_OPCODE in conf:
            cg.add(trig.set_opcode(conf[CONF_OPCODE]))
        if CONF_DATA in conf:
            cg.add(trig.set_data(conf[CONF_DATA]))

        await cg.register_component(trig, conf)
        await automation.build_automation(
            trig,
            [(cg.uint8, "source"), (cg.uint8, "destination"), (cg.std_vector.template(cg.uint8), "data")],
            conf,
        )
