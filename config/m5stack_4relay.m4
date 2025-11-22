define(`NAMESPACE', `m5stack_4relay')dnl
dnl This YAML is not intended for direct consumption by esphome.
dnl Because its YAML parser does not support anchors and aliases
dnl between files using !include, we get that feature by including
dnl files using m4.
dnl For example,
dnl   m4 m5stack_cores3.yaml m5stack_4relay.m4 example.m4 > example.yaml
dnl
dnl ESPHome support for M5Stack 4relay module
dnl   https://docs.m5stack.com/en/module/4relay
dnl
dnl a top level key for this NAMESPACE is created
dnl with keys and anchored content values.
dnl in order to affect an ESPHome configuration,
dnl such content should be aliased elsewhere.
dnl this should be done selectively so that the configuration only has what is needed.
dnl
.NAMESPACE:

  external_components: &NAMESPACE`'_external_components
    source:
      type: local
      path: ../components
    components: [m5stack_4relay_lgfx_, m5stack_4relay_esphome_]

undefine(`NAMESPACE')dnl
