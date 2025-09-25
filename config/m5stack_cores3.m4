define(`NAMESPACE', `m5stack_cores3')dnl
dnl This YAML is not intended for direct consumption by esphome.
dnl Because its YAML parser does not support anchors and aliases
dnl between files using !include, we get that feature by including
dnl files using m4.
dnl For example,
dnl   m4 m5stack_cores3.m4 example.m4 > example.yaml
dnl
dnl ESPHome support for M5Stack CoreS3 devices
dnl   https://docs.m5stack.com/en/core/CoreS3
dnl   https://docs.m5stack.com/en/core/M5CoreS3%20SE
dnl   https://docs.m5stack.com/en/core/CoreS3-Lite
dnl
dnl a top level key for this NAMESPACE is created
dnl with keys and anchored content values.
dnl in order to affect an ESPHome configuration,
dnl such content should be aliased elsewhere.
dnl this should be done selectively so that the configuration only has what is needed.
dnl
dnl content is inspired from
dnl   https://github.com/lboue/M5CoreS3-Esphome/blob/main/touch-and-sound/m5stack-cores3.yaml
dnl   https://github.com/lboue/M5CoreS3-Esphome/blob/main/voice-assistant/m5stack-cores3.yaml
dnl
dnl define the pins on the CoreS3's M5-Bus
define(M5STACK_CORE_M5_BUS_0_03, GPIO37)dnl
define(M5STACK_CORE_M5_BUS_0_04, GPIO35)dnl
define(M5STACK_CORE_M5_BUS_0_05, GPIO36)dnl
define(M5STACK_CORE_M5_BUS_0_06, GPIO44)dnl
define(M5STACK_CORE_M5_BUS_0_07, GPIO18)dnl
define(M5STACK_CORE_M5_BUS_0_08, GPIO12)dnl
define(M5STACK_CORE_M5_BUS_0_09, GPIO2)dnl
define(M5STACK_CORE_M5_BUS_0_10, GPIO6)dnl
define(M5STACK_CORE_M5_BUS_0_11, GPIO13)dnl
dnl
define(M5STACK_CORE_M5_BUS_1_00, GPIO10)dnl
define(M5STACK_CORE_M5_BUS_1_01, GPIO8)dnl
dnl
define(M5STACK_CORE_M5_BUS_1_03, GPIO5)dnl
define(M5STACK_CORE_M5_BUS_1_04, GPIO9)dnl
dnl
define(M5STACK_CORE_M5_BUS_1_06, GPIO43)dnl
define(M5STACK_CORE_M5_BUS_1_07, GPIO17)dnl
define(M5STACK_CORE_M5_BUS_1_08, GPIO11)dnl
define(M5STACK_CORE_M5_BUS_1_09, GPIO1)dnl
define(M5STACK_CORE_M5_BUS_1_10, GPIO7)dnl
define(M5STACK_CORE_M5_BUS_1_11, GPIO0)dnl
define(M5STACK_CORE_M5_BUS_1_12, GPIO14)dnl
.NAMESPACE:

  external_components: &NAMESPACE`'_external_components
    source:
      type: git
      url: https://github.com/m5stack/M5CoreS3-Esphome
    components:
      - board_m5cores3
      - m5cores3_audio
      - m5cores3_display
      - m5cores3_touchscreen
    refresh: 0s

  esphome: &NAMESPACE`'_esphome
    libraries:
      - m5stack/M5Unified@^0.1.11

  esp32: &NAMESPACE`'_esp32
    board: m5stack-cores3
    cpu_frequency: 240Mhz
    framework:
      type: esp-idf

  board_m5cores3: &NAMESPACE`'_board_m5cores3 {}

  spi: &NAMESPACE`'_spi
    id: spi_id
    interface: spi3   # spi2 is implicitly used by m5cores3_display
    clk_pin: GPIO36
    mosi_pin: GPIO37

  display: &NAMESPACE`'_display
    platform: m5cores3_display
    model: ILI9342
    dc_pin: GPIO35

  i2c: &NAMESPACE`'_i2c
    scl: GPIO11
    sda: GPIO12
    scan: true
    id: i2c_id
    frequency: 400kHz

  touchscreen: &NAMESPACE`'_touchscreen
    platform: m5cores3_touchscreen

  m5cores3_audio: &NAMESPACE`'_m5cores3_audio
    id: m5cores3_audio

  microphone: &NAMESPACE`'_microphone
    platform: m5cores3_audio
    m5cores3_audio_id: m5cores3_audio
    adc_type: external
    i2s_din_pin: 14
    pdm: false

  speaker: &NAMESPACE`'_speaker
    platform: m5cores3_audio
    m5cores3_audio_id: m5cores3_audio
    dac_type: external
    i2s_dout_pin: 13
    mode: mono

undefine(`NAMESPACE')dnl
