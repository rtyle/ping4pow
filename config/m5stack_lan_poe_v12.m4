define(`NAMESPACE', `m5stack_lan_poe_v12')dnl
dnl This YAML is not intended for direct consumption by esphome.
dnl Because its YAML parser does not support anchors and aliases
dnl between files using !include, we get that feature by including
dnl files using m4.
dnl For example,
dnl   m4 m5stack_cores3.yaml m5stack_lan_poe_v12.m4 example.m4 > example.yaml
dnl
dnl ESPHome support for M5Stack Base LAN PoE v1.2
dnl   https://docs.m5stack.com/en/base/lan_poe_v12
dnl
dnl a top level key for this NAMESPACE is created
dnl with keys and anchored content values.
dnl in order to affect an ESPHome configuration,
dnl such content should be aliased elsewhere.
dnl this should be done selectively so that the configuration only has what is needed.
.NAMESPACE:

define(NAMESPACE`'_common, dnl
`    type: W5500
    cs_pin: M5STACK_CORE_M5_BUS_1_04
    reset_pin: M5STACK_CORE_M5_BUS_1_10
    interrupt_pin: M5STACK_CORE_M5_BUS_1_12')dnl
  ethernet: &NAMESPACE`'_ethernet
indir(NAMESPACE`'_common)
    mosi_pin: M5STACK_CORE_M5_BUS_0_03
    miso_pin: M5STACK_CORE_M5_BUS_0_04
    clk_pin: M5STACK_CORE_M5_BUS_0_05

  # use with m5cores3_display (with appropriate M5-Bus re-routing adapter)
  ethernet_m5cores3: &NAMESPACE`'_ethernet_m5cores3_display
indir(NAMESPACE`'_common)
    mosi_pin: GPIO10
    miso_pin: GPIO8
    clk_pin: GPIO6
    interface: spi3  # spi2 is implicitly used by m5cores3_display

undefine(`NAMESPACE'_common)dnl
undefine(`NAMESPACE')dnl
