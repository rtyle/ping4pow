dnl NAME
dnl   ping4pow - ping hosts as a condition for powering a load
dnl
dnl SYNOPSIS
dnl   m4 [options] ping4pow.m4 > ping4pow.yaml
dnl
dnl DESCRIPTION
dnl   Use m4 to preprocess this YAML for esphome
dnl   because esphome's YAML parser does not support anchors and aliases
dnl   between files using !include.
dnl   m4 macro definitions and expansions are used to enhance readability
dnl   and maintainability.
dnl
dnl OPTIONS
dnl   -DNAME=name
dnl     Value of the name key of the esphome component.
dnl     This will be used for the mDNS name of the device.
ifdef(`NAME', `', `define(`NAME', `ping4pow')')dnl
dnl
dnl   -DHOSTS=hosts
dnl     Name of the file that defines the hosts to be pinged.
dnl     Each line should define a host by invoking the host macro
dnl     with the host's address and name.
ifdef(`HOSTS', `', `define(`HOSTS', `hosts.m4')')dnl
dnl
dnl   -DLOGGER_LEVEL=logger_level
dnl     Value of the level key of the logger component.
ifdef(`LOGGER_LEVEL', `', `define(`LOGGER_LEVEL', `INFO')')dnl
dnl
dnl   -DGPIO_RELAY
dnl     Control the loads NC relay through a GPIO pin
dnl     as opposed to an M5Stack 4Relay module.
dnl
dnl   -DSMTP=smtp
dnl     Value is the name of the file that declares the smtp_ component
ifdef(`SMTP', `', `define(`SMTP', `smtp.m4')')dnl
---

include(m5stack_cores3.m4)dnl
include(m5stack_4relay.m4)dnl
include(m5stack_lan_poe_v12.m4)dnl
external_components:
  - <<: *m5stack_cores3_external_components
  - <<: *m5stack_4relay_external_components
  - source:
      type: local
      path: ../components
    components: [
      format_, m5cores3_touchscreen_, ping_, rotation_, since_, smtp_]

esphome:
  <<: *m5stack_cores3_esphome
  name: NAME

define(`target_count', 16)dnl
esp32:
  board: m5stack-cores3
  cpu_frequency: 240Mhz
  framework:
    type: esp-idf
    sdkconfig_options:
      CONFIG_LWIP_MAX_SOCKETS: "eval(10 + 8 + target_count)"
      CONFIG_LWIP_MAX_RAW_PCBS: "eval(16 + target_count)"
undefine(`target_count')dnl

board_m5cores3: *m5stack_cores3_board_m5cores3

logger:
  level: LOGGER_LEVEL

ethernet: *m5stack_lan_poe_v12_ethernet_m5cores3_display

ota:
  platform: esphome

api:
  reboot_timeout: 0s
  encryption:
    key: !secret NAME-api-key

web_server:
  version: 3
  sorting_groups:
    - id: state_group_
      name: State
      sorting_weight: 0.0
    - id: ping_summary_group_
      name: Ping Summary
      sorting_weight: 0.1
    - id: ping_target_group_
      name: Ping Targets
      sorting_weight: 0.2
    - id: power_group_
      name: Power
      sorting_weight: 0.3
    - id: uptime_group_
      name: Uptime
      sorting_weight: 0.4

debug:
  update_interval: 60s

text_sensor:
  - platform: debug
    device:
      name: debug device
    reset_reason:
      name: debug reset_reason

sensor:
  - platform: debug
    free:
      name: debug free
    block:
      name: debug block
    loop_time:
      name: debug loop_time
    psram:
      name: debug psram
    cpu_frequency:
      name: debug cpu_frequency

binary_sensor:

define(`_repeat', `ifelse(0, `$1', `', `$2`'_repeat(decr(`$1'), `$2')')')dnl
define(`_indent', `_repeat(`$1', `  ')')dnl
define(`_smtp_send_', `_indent($1)- smtp_.send:
_indent(eval(2+$1))subject: NAME $2
')dnl
define(`_smtp_send', `')dnl
define(`_smtp_define', `$1`'define(`_smtp_send', defn(`_smtp_send_'))')dnl
sinclude(SMTP)dnl
undefine(`_smtp_define')dnl
dnl
ifdef(`GPIO_RELAY', `', `dnl
m5stack_4relay_lgfx_:
  - id: relays_
    relays:
      - id: power_
        index: 0
        name: power
        inverted: true
        icon: mdi:power
        web_server:
          sorting_group_id: power_group_
          sorting_weight: 0
        on_state:
          lvgl.widget.update:
            id: power_widget_
            state:
              checked: !lambda return x;'

)dnl
globals:
  - id: state_0_off_count_
    type: int
    initial_value: "0"

script:
  # each switch in our state machine executes enter_ for their state on_turn_on.
  # all other states are exited.
  - id: enter_
    parameters:
      state: int
    then:
      - lambda: |-
          static std::array switch_button{
            std::make_tuple(id(state_0_), id(state_0_widget_), false),
            std::make_tuple(id(state_1_), id(state_1_widget_), false),
            std::make_tuple(id(state_2_), id(state_2_widget_), true),
            std::make_tuple(id(state_3_), id(state_3_widget_), false),
            std::make_tuple(id(state_4_), id(state_4_widget_), true),
            std::make_tuple(id(state_5_), id(state_5_widget_), false),
          };
          size_t index{0};
          for (auto [_switch, button, hidden]: switch_button) {
            if (index == state) {
              if (hidden) {
                lv_obj_clear_flag(button, LV_OBJ_FLAG_HIDDEN);
              }
            }
            else {
              _switch->turn_off();
              if (hidden) {
                lv_obj_add_flag(button, LV_OBJ_FLAG_HIDDEN);
              }
            }
            ++index;
          }

switch:
ifdef(`GPIO_RELAY', `dnl
  - id: power_
    name: power
    platform: gpio
    pin:
      number: M5STACK_CORE_M5_BUS_1_03
      inverted: true
    restore_mode: ALWAYS_ON
    icon: mdi:power
    web_server:
      sorting_group_id: power_group_
      sorting_weight: 0
    on_state:
      lvgl.widget.update:
        id: power_widget_
        state:
          checked: !lambda return x;'

)dnl
  # the following switches reflect states in a state machine.
  # they cooperate so that only one switch/state is on at a time.
  # we start at state 1, conditionally advance through 2, 3 & 4
  # and when 5 is done we enter state 1 again.
  # for testing, these states may be entered through a UI.
  # entering state 0 will stop the machine until state 0 is exited.
  - id: state_0_
    platform: lvgl
    widget: state_0_widget_
    name: stop
    restore_mode: ALWAYS_OFF
    icon: mdi:cog
    web_server:
      sorting_group_id: state_group_
      sorting_weight: 0
    on_turn_on:
      - script.execute:
          id: enter_
          state: 0
    on_turn_off:
      # regardless of the restored state of switch 1,
      # if we try to turn it on now, its on_turn_on will not be invoked!
      if:
        condition:
          lambda: return id(state_0_off_count_)++;
        then:
          - switch.turn_on: state_1_
  - id: state_1_
    platform: lvgl
    widget: state_1_widget_
    name: wait for ping none
    restore_mode: ALWAYS_ON
    icon: mdi:network-off
    web_server:
      sorting_group_id: state_group_
      sorting_weight: 1
    on_turn_on:
      - script.execute:
          id: enter_
          state: 1
      - wait_until:
          condition:
            or:
              - switch.is_off: state_1_
              - binary_sensor.is_on: ping_none_
      - if:
          condition:
            switch.is_on: state_1_
          then:
            - switch.turn_on: state_2_
  - id: state_2_
    platform: lvgl
    widget: state_2_widget_
    name: wait while ping none
    restore_mode: ALWAYS_OFF
    icon: mdi:dots-horizontal
    web_server:
      sorting_group_id: state_group_
      sorting_weight: 2
    on_turn_on:
      - script.execute:
          id: enter_
          state: 2
      - wait_until:
          condition:
            or:
              - switch.is_off: state_2_
              - binary_sensor.is_off: ping_none_
          timeout: 10s
      - if:
          condition:
            switch.is_on: state_2_
          then:
            - if:
                condition:
                  binary_sensor.is_off: ping_none_
                then:
                  - switch.turn_on: state_1_
                else:
                  - switch.turn_on: state_3_
  - id: state_3_
    platform: lvgl
    widget: state_3_widget_
    name: wait for ping all
    restore_mode: ALWAYS_OFF
    icon: mdi:network
    web_server:
      sorting_group_id: state_group_
      sorting_weight: 3
    on_turn_on:
      - script.execute:
          id: enter_
          state: 3
      - wait_until:
          condition:
            or:
              - switch.is_off: state_3_
              - binary_sensor.is_on: ping_all_
      - if:
          condition:
            switch.is_on: state_3_
          then:
            - switch.turn_on: state_4_
  - id: state_4_
    platform: lvgl
    widget: state_4_widget_
    name: wait while ping all
    restore_mode: ALWAYS_OFF
    icon: mdi:dots-horizontal
    web_server:
      sorting_group_id: state_group_
      sorting_weight: 4
    on_turn_on:
      - script.execute:
          id: enter_
          state: 4
      - wait_until:
          condition:
            or:
              - switch.is_off: state_4_
              - binary_sensor.is_off: ping_all_
          timeout: 60s
      - if:
          condition:
            switch.is_on: state_4_
          then:
            - if:
                condition:
                  binary_sensor.is_off: ping_all_
                then:
                  - switch.turn_on: state_3_
                else:
                  - switch.turn_on: state_5_
  - id: state_5_
    platform: lvgl
    widget: state_5_widget_
    name: power cycle
    restore_mode: ALWAYS_OFF
    icon: mdi:power-cycle
    web_server:
      sorting_group_id: state_group_
      sorting_weight: 5
    on_turn_on:
      - script.execute:
          id: enter_
          state: 5
      - switch.turn_off: power_
      - lambda: !lambda id(power_since_)->set_when();
_smtp_send(3, power cycle)`'dnl
      - wait_until:
          condition:
            switch.is_off: state_5_
          timeout: 10s
      - if:
          condition:
            switch.is_on: state_5_
          then:
            - switch.turn_on: power_
            - lambda: !lambda id(power_since_)->set_when();
            - switch.turn_on: state_1_

display:
  - <<: *m5stack_cores3_display
    id: display_
    auto_clear_enabled: false
    update_interval: 1ms

touchscreen:
  - id: touchscreen_
    platform: m5cores3_touchscreen_
    update_interval: 50ms
    left:
      id: brightness_decrement_
      on_press:
        number.decrement:
          id: brightness_
          cycle: false
    center:
      id: home_
      on_press:
        lambda: |-
          auto tileview{id(tileview_)};
          lv_obj_set_tile(tileview, id(state_tile_widget_), LV_ANIM_OFF);
          lv_event_send(tileview, LV_EVENT_VALUE_CHANGED, nullptr);
          id(tile_iterator_).reset();
    right:
      id: brightness_increment_
      on_press:
        number.increment:
          id: brightness_
          cycle: false

define(mdi_arrow_left_bold, \U000F0731)dnl
define(mdi_arrow_right_bold, \U000F0734)dnl
define(mdi_arrow_up_bold_circle, \U000F005F)dnl
define(mdi_check_network, \U000F0C53)dnl
define(mdi_cog, \U000F0493)dnl
define(mdi_cog_stop, \U000F1937)dnl
define(mdi_cog_pause, \U000F1933)dnl
define(mdi_dots_horizontal, \U000F01D8)dnl
define(mdi_lock, \U000F033E)dnl
define(mdi_lock_open, \U000F033F)dnl
define(mdi_map_marker, \U000F034E)dnl
define(mdi_network, \U000F06F3)dnl
define(mdi_network_off, \U000F0C9B)dnl
define(mdi_power, \U000F0425)dnl
define(mdi_power_cycle, \U000F0901)dnl
define(mdi_tag, \U000F04F9)dnl
font:
  - id: font_
    file: "gfonts://Roboto"
    size: 32
    bpp: 4
    glyphsets:
      - GF_Latin_Kernel
      - GF_Latin_Core
    extras:
      - file: "https://raw.githubusercontent.com/Templarian/\
                MaterialDesign-Webfont/master/fonts/\
                materialdesignicons-webfont.ttf"
        glyphs:
          - "mdi_arrow_left_bold"
          - "mdi_arrow_right_bold"
          - "mdi_arrow_up_bold_circle"
          - "mdi_check_network"
          - "mdi_cog"
          - "mdi_cog_stop"
          - "mdi_cog_pause"
          - "mdi_dots_horizontal"
          - "mdi_lock"
          - "mdi_lock_open"
          - "mdi_map_marker"
          - "mdi_network"
          - "mdi_network_off"
          - "mdi_power"
          - "mdi_power_cycle"
          - "mdi_tag"
  - id: font_small_
    file: "gfonts://Roboto"
    size: 18
    bpp: 4
    glyphsets:
      - GF_Latin_Kernel
      - GF_Latin_Core
    extras:
      - file: "https://raw.githubusercontent.com/Templarian/\
                MaterialDesign-Webfont/master/fonts/\
                materialdesignicons-webfont.ttf"
        glyphs:
          - "mdi_arrow_left_bold"
          - "mdi_arrow_right_bold"
          - "mdi_arrow_up_bold_circle"
          - "mdi_check_network"
          - "mdi_cog"
          - "mdi_cog_stop"
          - "mdi_cog_pause"
          - "mdi_dots_horizontal"
          - "mdi_lock"
          - "mdi_lock_open"
          - "mdi_map_marker"
          - "mdi_network"
          - "mdi_network_off"
          - "mdi_power"
          - "mdi_power_cycle"
          - "mdi_tag"

format_:

since_:
  - id: boot_since_
    name: since boot
    when: 0ns
    icon: mdi:arrow-up-bold-circle
    web_server:
      sorting_group_id: uptime_group_
      sorting_weight: 0
    label: boot_since_widget_
    text:
      name: since boot (D HH:MM:SS)
      icon: mdi:arrow-up-bold-circle
      web_server:
        sorting_group_id: uptime_group_
        sorting_weight: 1
  - id: power_since_
    name: since power cycle
    icon: mdi:power-cycle
    web_server:
      sorting_group_id: power_group_
      sorting_weight: 1
    label: power_since_widget_
    text:
      name: since power cycle (D HH:MM:SS)
      icon: mdi:power-cycle
      web_server:
        sorting_group_id: power_group_
        sorting_weight: 2

ping_:
  - none:
      id: ping_none_
      name: ping none
      icon: mdi:network-off
      web_server:
        sorting_group_id: ping_summary_group_
        sorting_weight: 0
      on_state:
        lvgl.widget.update:
          id: ping_none_widget_
          state:
            checked: !lambda return x;
    some:
      id: ping_some_
      name: ping some
      icon: mdi:circle-half-full
      web_server:
        sorting_group_id: ping_summary_group_
        sorting_weight: 1
      on_state:
        lvgl.widget.update:
          id: ping_some_widget_
          state:
            checked: !lambda return x;
    all:
      id: ping_all_
      name: ping all
      icon: mdi:network
      web_server:
        sorting_group_id: ping_summary_group_
        sorting_weight: 2
      on_state:
        lvgl.widget.update:
          id: ping_all_widget_
          state:
            checked: !lambda return x;
    count:
      id: ping_count_
      name: ping count
      icon: mdi:pound
      web_server:
        sorting_group_id: ping_summary_group_
        sorting_weight: 3
      on_value:
        lvgl.label.update:
          id: ping_some_label_widget_
          text: !lambda return to_string(static_cast<int>(x));
    since:
      id: ping_since_change_
      name: ping since change
      icon: mdi:check-network
      web_server:
        sorting_group_id: ping_summary_group_
        sorting_weight: 4
      label: ping_since_change_widget_
      text:
        name: ping since change (D HH:MM:SS)
        icon: mdi:check-network
        web_server:
          sorting_group_id: ping_summary_group_
          sorting_weight: 5
    targets:
define(`__increment', `define(`$1', incr($1))')dnl
define(`__count', `-1')dnl
define(host, `__increment(`__count')dnl
      - id: ping_`'__count`'_
        name: ping __count ($1 $2)
        address: $1
        interval: 10s
        timeout: 2s
        icon: mdi:cog
        web_server:
          sorting_group_id: ping_target_group_
          sorting_weight: __count
        on_state:
          - lvgl.widget.update:
              id: ping_`'__count`'_widget_
              state:
                checked: !lambda return x;
          - lvgl.label.update:
              id: ping_`'__count`'_label_widget_
              text: !lambda |-
                return std::string{
                  x ? "mdi_cog" : "mdi_cog_stop"};
        able:
          id: ping_`'__count`'_able_
          name: ping __count able
          icon: mdi:network
          web_server:
            sorting_group_id: ping_target_group_
            sorting_weight: __count
          on_state:
            - lvgl.widget.update:
                id: ping_`'__count`'_able_widget_
                state:
                  checked: !lambda return x;
            - lvgl.label.update:
                id: ping_`'__count`'_label_able_widget_
                text: !lambda |-
                  return std::string{
                    x ? "mdi_network" : "mdi_network_off"};
        since:
          id: ping_`'__count`'_since_change_
          name: ping __count since change
          icon: mdi:check-network
          web_server:
            sorting_group_id: ping_target_group_
            sorting_weight: __count
          label: ping_`'__count`'_since_change_widget_
          text:
            name: ping __count since change (D HH:MM:SS)
            icon: mdi:check-network
            web_server:
              sorting_group_id: ping_target_group_
              sorting_weight: __count')dnl
include(HOSTS)dnl
undefine(`host')dnl
undefine(`__count')dnl

define(_bg_on, 0x7D70F2)dnl
define(_fg_on, 0x1A0D4D)dnl
define(_bg_off, 0x3D2F80)dnl
define(_fg_off, 0xD4CBFF)dnl
define(_bg_on_power, 0xF27070)dnl
define(_fg_on_power, 0x4D0D0D)dnl
define(_bg_off_power, 0x802F2F)dnl
define(_fg_off_power, 0xFFCBCB)dnl
lvgl:
  displays: [display_]
  touchscreens:
    - touchscreen_
  theme:
    obj:
      bg_color: _bg_off
      border_width: 0
      radius: 0
      pad_all: 0
      text_font: font_
    button:
      border_width: 2
      text_color: _fg_off
      bg_color: _bg_off
      checked:
        bg_color: _bg_on
        text_color: _fg_on
        border_color: _fg_off
      checkable: true
      grid_cell_x_align: STRETCH
      grid_cell_y_align: STRETCH
      align: center
    label:
      grid_cell_x_align: center
      grid_cell_y_align: center
      align: center
  style_definitions:
    - id: label_style_widget_
      bg_color: _bg_off
      text_color: _fg_off
  pages:
    - widgets:
        - obj:
            width: 100%
            height: 100%
            layout:
              type: flex
              flex_flow: column
              pad_row: 0
            widgets:
              - obj:
                  flex_grow: 5
                  width: 100%
                  widgets:
                    - tileview:
                        id: tileview_
                        width: 100%
                        height: 100%
                        bg_color: _bg_off
                        tiles:
                          - id: state_tile_widget_
                            row: 0
                            column: 0
                            dir: HOR
                            layout:
                              type: GRID
                              grid_columns: [
                                FR(1), FR(1), FR(1), FR(1), FR(1), FR(1)]
                              grid_rows: [FR(1), FR(1), FR(1)]
                            widgets:
                              - button:
                                  id: state_1_widget_
                                  grid_cell_column_pos: 0
                                  grid_cell_column_span: 3
                                  grid_cell_row_pos: 0
                                  checkable: true
                                  widgets:
                                    - label:
                                        text: "mdi_cog_pause`'mdi_network_off"
                              - button:
                                  id: state_2_widget_
                                  grid_cell_column_pos: 0
                                  grid_cell_column_span: 3
                                  grid_cell_row_pos: 0
                                  checkable: true
                                  hidden: true
                                  widgets:
                                    - label:
                                        text: "mdi_cog_pause`'mdi_network_off`'mdi_dots_horizontal"
                              - button:
                                  id: state_3_widget_
                                  grid_cell_column_pos: 3
                                  grid_cell_column_span: 3
                                  grid_cell_row_pos: 0
                                  checkable: true
                                  widgets:
                                    - label:
                                        text: "mdi_cog_pause`'mdi_network"
                              - button:
                                  id: state_4_widget_
                                  grid_cell_column_pos: 3
                                  grid_cell_column_span: 3
                                  grid_cell_row_pos: 0
                                  checkable: true
                                  hidden: true
                                  widgets:
                                    - label:
                                        text: "mdi_cog_pause`'mdi_network`'mdi_dots_horizontal"
                              - button:
                                  id: state_0_widget_
                                  grid_cell_column_pos: 0
                                  grid_cell_column_span: 2
                                  grid_cell_row_pos: 1
                                  checkable: true
                                  widgets:
                                    - label:
                                        id: state_0_label_widget_
                                        text: "mdi_cog"
                                  on_value:
                                    lvgl.label.update:
                                      id: state_0_label_widget_
                                      text: !lambda |-
                                        return std::string{
                                          x ? "mdi_cog_stop" : "mdi_cog"};
                              - button:
                                  id: power_widget_
                                  grid_cell_column_pos: 2
                                  grid_cell_column_span: 2
                                  grid_cell_row_pos: 1
                                  checkable: true
                                  bg_color: _bg_off_power
                                  text_color: _fg_off_power
                                  border_width: 4
                                  checked:
                                    bg_color: _bg_on_power
                                    text_color: _fg_on_power
                                    border_color: _fg_off_power
                                  widgets:
                                    - label:
                                        text: "mdi_power"
                                  on_value:
                                    if:
                                      condition:
                                        lambda: return x;
                                      then:
                                        - switch.turn_on: power_
                                      else:
                                        - switch.turn_off: power_
                              - button:
                                  id: state_5_widget_
                                  grid_cell_column_pos: 4
                                  grid_cell_column_span: 2
                                  grid_cell_row_pos: 1
                                  checkable: true
                                  widgets:
                                    - label:
                                        text: "mdi_power_cycle"
                              - button:
                                  id: ping_none_widget_
                                  grid_cell_column_pos: 0
                                  grid_cell_column_span: 2
                                  grid_cell_row_pos: 2
                                  checkable: true
                                  clickable: false
                                  checked:
                                    border_color: _fg_off
                                  widgets:
                                    - label:
                                        text: "\U000F0C9B"
                              - button:
                                  id: ping_some_widget_
                                  grid_cell_column_pos: 2
                                  grid_cell_column_span: 2
                                  grid_cell_row_pos: 2
                                  checkable: true
                                  clickable: false
                                  checked:
                                    border_color: _fg_off
                                  widgets:
                                    - label:
                                        id: ping_some_label_widget_
                                        text: "0"
                              - button:
                                  id: ping_all_widget_
                                  grid_cell_column_pos: 4
                                  grid_cell_column_span: 2
                                  grid_cell_row_pos: 2
                                  checkable: true
                                  clickable: false
                                  checked:
                                    border_color: _fg_off
                                  widgets:
                                    - label:
                                        text: "mdi_network"
                          - id: since_tile_widget_
                            row: 0
                            column: 1
                            dir: HOR
                            layout:
                              type: GRID
                              grid_columns: [FR(1), FR(5)]
                              grid_rows: [FR(1), FR(1), FR(1)]
                            widgets:
                              - label:
                                  styles: label_style_widget_
                                  grid_cell_column_pos: 0
                                  grid_cell_row_pos: 0
                                  text: "mdi_check_network"
                              - label:
                                  styles: label_style_widget_
                                  id: ping_since_change_widget_
                                  grid_cell_column_pos: 1
                                  grid_cell_row_pos: 0
                                  text: "NA"
                              - label:
                                  styles: label_style_widget_
                                  grid_cell_column_pos: 0
                                  grid_cell_row_pos: 1
                                  text: "mdi_power_cycle"
                              - label:
                                  styles: label_style_widget_
                                  id: power_since_widget_
                                  grid_cell_column_pos: 1
                                  grid_cell_row_pos: 1
                                  text: "NA"
                              - label:
                                  styles: label_style_widget_
                                  grid_cell_column_pos: 0
                                  grid_cell_row_pos: 2
                                  text: "mdi_arrow_up_bold_circle"
                              - label:
                                  styles: label_style_widget_
                                  id: boot_since_widget_
                                  grid_cell_column_pos: 1
                                  grid_cell_row_pos: 2
                                  text: "NA"
define(`__count', `-1')dnl
define(host, `__increment(`__count')dnl
                          - id: ping_`'__count`'_tile_widget_
                            row: 0
                            column: eval(__count + 2)
                            dir: HOR
                            layout:
                              type: GRID
                              grid_columns:
                                - FR(1)
                                - FR(1)
                                - FR(1)
                                - FR(1)
                                - FR(1)
                                - FR(1)
                              grid_rows: [FR(1), FR(1), FR(1), FR(1)]
                            widgets:
                              - button:
                                  id: ping_`'__count`'_widget_
                                  grid_cell_column_pos: 0
                                  grid_cell_row_pos: 0
                                  grid_cell_column_span: 3
                                  checkable: true
                                  widgets:
                                    - label:
                                        id: ping_`'__count`'_label_widget_
                                        text: "mdi_cog_stop"
                                  on_value:
                                    switch.control:
                                      id: ping_`'__count`'_
                                      state: !lambda return x;
                              - button:
                                  id: ping_`'__count`'_able_widget_
                                  grid_cell_column_pos: 3
                                  grid_cell_row_pos: 0
                                  grid_cell_column_span: 3
                                  checkable: true
                                  widgets:
                                    - label:
                                        id: ping_`'__count`'_label_able_widget_
                                        text: "mdi_network_off"
                              - label:
                                  styles: label_style_widget_
                                  grid_cell_column_pos: 0
                                  grid_cell_row_pos: 1
                                  text: "mdi_check_network"
                              - label:
                                  styles: label_style_widget_
                                  id: ping_`'__count`'_since_change_widget_
                                  grid_cell_column_pos: 1
                                  grid_cell_row_pos: 1
                                  grid_cell_column_span: 5
                                  text: "NA"
                              - label:
                                  styles: label_style_widget_
                                  grid_cell_column_pos: 0
                                  grid_cell_row_pos: 2
                                  text: "mdi_map_marker"
                              - label:
                                  styles: label_style_widget_
                                  grid_cell_column_pos: 1
                                  grid_cell_row_pos: 2
                                  grid_cell_column_span: 5
                                  text: "$1"
                              - label:
                                  styles: label_style_widget_
                                  grid_cell_column_pos: 0
                                  grid_cell_row_pos: 3
                                  text: "mdi_tag"
                              - label:
                                  styles: label_style_widget_
                                  grid_cell_column_pos: 1
                                  grid_cell_row_pos: 3
                                  grid_cell_column_span: 5
                                  text: "$2"')dnl
include(HOSTS)dnl
undefine(`host')dnl
undefine(`__count')dnl
                    - button:
                        id: unlock_overlay_widget_
                        width: 100%
                        height: 100%
                        bg_opa: transp
                        border_width: 0
              - obj:
                  flex_grow: 1
                  width: 100%
                  layout:
                    type: flex
                    flex_flow: row
                    pad_column: 0
                  widgets:
                    - button:
                        flex_grow: 1
                        height: 100%
                        widgets:
                          - label:
                              text_font: font_small_
                              text: "mdi_arrow_left_bold"
                        on_click:
                          lambda: |-
                            auto tileview{id(tileview_)};
                            auto it{id(tile_iterator_)};
                            auto tile{reinterpret_cast<lv_obj_t *>(*--(*it))};
                            lv_obj_set_tile(tileview, tile, LV_ANIM_OFF);
                            lv_event_send(
                              tileview, LV_EVENT_VALUE_CHANGED, nullptr);
                    - button:
                        flex_grow: 1
                        height: 100%
                        checkable: true
                        widgets:
                          - label:
                              id: unlock_label_widget_
                              text_font: font_small_
                              text: "mdi_lock"
                        on_value:
                          - lvgl.widget.update:
                              id: unlock_overlay_widget_
                              hidden: !lambda return x;
                          - lvgl.label.update:
                              id: unlock_label_widget_
                              text: !lambda |-
                                return std::string{
                                  x ? "mdi_lock_open" : "mdi_lock"};
                    - button:
                        flex_grow: 1
                        height: 100%
                        widgets:
                          - label:
                              text_font: font_small_
                              text: "mdi_arrow_right_bold"
                        on_click:
                          lambda: |-
                            auto tileview{id(tileview_)};
                            auto it{id(tile_iterator_)};
                            auto tile{reinterpret_cast<lv_obj_t *>(*++(*it))};
                            lv_obj_set_tile(tileview, tile, LV_ANIM_OFF);
                            lv_event_send(
                              tileview, LV_EVENT_VALUE_CHANGED, nullptr);

rotation_:
  - id: tile_rotation_
    items:
      - state_tile_widget_
      - since_tile_widget_
define(`__count', `-1')dnl
define(host, `__increment(`__count')dnl
      - ping_`'__count`'_tile_widget_')dnl
include(HOSTS)dnl
undefine(`host')dnl
undefine(`__count')dnl
    iterators:
      - id: tile_iterator_

number:
  - platform: template
    id: brightness_
    min_value: 0
    max_value: 255
    step: 32
    mode: slider
    optimistic: true
    restore_value: true
    initial_value: 127
    on_value:
      lambda: !lambda M5.Display.setBrightness(x);
