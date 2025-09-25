dnl This YAML is not intended for direct consumption by esphome.
dnl Because its YAML parser does not support anchors and aliases
dnl between files using !include, we get that feature by including
dnl files using m4.
dnl   m4 ping4pow.m4 > ping4pow.yaml
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
    components: [_format, _m5cores3_touchscreen, _ping, _rotation, _since]

esphome:
  <<: *m5stack_cores3_esphome
  name: ping4pow

esp32: *m5stack_cores3_esp32

board_m5cores3: *m5stack_cores3_board_m5cores3

logger:
  level: INFO

ethernet: *m5stack_lan_poe_v12_ethernet_m5cores3_display

ota:
  platform: esphome
api:
  reboot_timeout: 0s
web_server:
  version: 3
  sorting_groups:
    - id: _state_group
      name: State
      sorting_weight: 0.0
    - id: _ping_summary_group
      name: Ping Summary
      sorting_weight: 0.1
    - id: _ping_target_group
      name: Ping Targets
      sorting_weight: 0.2
    - id: _power_group
      name: Power
      sorting_weight: 0.3
    - id: _uptime_group
      name: Uptime
      sorting_weight: 0.4

debug:
  update_interval: 10s

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

ifdef(`gpio_relay', `', `dnl
_m5stack_4relay_lgfx:
  - id: _relays
    relays:
      - id: _power
        index: 0
        name: power
        inverted: true
        icon: mdi:power
        web_server:
          sorting_group_id: _power_group
          sorting_weight: 0
        on_state:
          lvgl.widget.update:
            id: __power
            state:
              checked: !lambda return x;'
)dnl
globals:
  - id: _0_off_count
    type: int
    initial_value: "0"

script:
  # each switch in our state machine executes _enter for their state on_turn_on.
  # all other states are exited.
  - id: _enter
    parameters:
      state: int
    then:
      - lambda: |-
          static std::array switch_button{
            std::make_pair(id(_state_0), id(__state_0)),
            std::make_pair(id(_state_1), id(__state_1)),
            std::make_pair(id(_state_2), id(__state_2)),
            std::make_pair(id(_state_3), id(__state_3)),
            std::make_pair(id(_state_4), id(__state_4)),
            std::make_pair(id(_state_5), id(__state_5)),
          };
          size_t index{0};
          for (auto [_switch, button]: switch_button) {
            if (index == state) {
              if (2 == index || 4 == index) {
                lv_obj_clear_flag(button, LV_OBJ_FLAG_HIDDEN);
              }
            }
            else {
              _switch->turn_off();
              if (2 == index || 4 == index) {
                lv_obj_add_flag(button, LV_OBJ_FLAG_HIDDEN);
              }
            }
            ++index;
          }

switch:
ifdef(`gpio_relay', `dnl
  - id: _power
    name: power
    platform: gpio
    pin:
      number: M5STACK_CORE_M5_BUS_1_03
      inverted: true
    restore_mode: RESTORE_DEFAULT_OFF
    icon: mdi:power
    web_server:
      sorting_group_id: _power_group
      sorting_weight: 0
    on_state:
      lvgl.widget.update:
        id: __power
        state:
          checked: !lambda return x;'

)dnl
  # the following switches reflect states in a state machine.
  # they cooperate so that only one switch/state is on at a time.
  # we start at state 1, conditionally advance through 2, 3 & 4
  # and when 5 is done we enter state 1 again.
  # for testing, these states may be entered through a UI.
  # entering state 0 will stop the machine until state 0 is exited.
  - id: _state_0
    platform: lvgl
    widget: __state_0
    name: stop
    restore_mode: ALWAYS_OFF
    icon: mdi:cog
    web_server:
      sorting_group_id: _state_group
      sorting_weight: 0
    on_turn_on:
      - script.execute:
          id: _enter
          state: 0
    on_turn_off:
      # regardless of the restored state of switch 1,
      # if we try to turn it on now, its on_turn_on will not be invoked!
      if:
        condition:
          lambda: return id(_0_off_count)++;
        then:
          - switch.turn_on: _state_1
  - id: _state_1
    platform: lvgl
    widget: __state_1
    name: wait for ping none
    restore_mode: ALWAYS_ON
    icon: mdi:network-off
    web_server:
      sorting_group_id: _state_group
      sorting_weight: 1
    on_turn_on:
      - script.execute:
          id: _enter
          state: 1
      - wait_until:
          condition:
            or:
              - switch.is_off: _state_1
              - binary_sensor.is_on: _ping_none
      - if:
          condition:
            switch.is_on: _state_1
          then:
            - switch.turn_on: _state_2
  - id: _state_2
    platform: lvgl
    widget: __state_2
    name: wait while ping none
    restore_mode: ALWAYS_OFF
    icon: mdi:dots-horizontal
    web_server:
      sorting_group_id: _state_group
      sorting_weight: 2
    on_turn_on:
      - script.execute:
          id: _enter
          state: 2
      - wait_until:
          condition:
            or:
              - switch.is_off: _state_2
              - binary_sensor.is_off: _ping_none
          timeout: 20s
      - if:
          condition:
            switch.is_on: _state_2
          then:
            - if:
                condition:
                  binary_sensor.is_off: _ping_none
                then:
                  - switch.turn_on: _state_1
                else:
                  - switch.turn_on: _state_3
  - id: _state_3
    platform: lvgl
    widget: __state_3
    name: wait for ping all
    restore_mode: ALWAYS_OFF
    icon: mdi:network
    web_server:
      sorting_group_id: _state_group
      sorting_weight: 3
    on_turn_on:
      - script.execute:
          id: _enter
          state: 3
      - wait_until:
          condition:
            or:
              - switch.is_off: _state_3
              - binary_sensor.is_on: _ping_all
      - if:
          condition:
            switch.is_on: _state_3
          then:
            - switch.turn_on: _state_4
  - id: _state_4
    platform: lvgl
    widget: __state_4
    name: wait while ping all
    restore_mode: ALWAYS_OFF
    icon: mdi:dots-horizontal
    web_server:
      sorting_group_id: _state_group
      sorting_weight: 4
    on_turn_on:
      - script.execute:
          id: _enter
          state: 4
      - wait_until:
          condition:
            or:
              - switch.is_off: _state_4
              - binary_sensor.is_off: _ping_all
          timeout: 60s
      - if:
          condition:
            switch.is_on: _state_4
          then:
            - if:
                condition:
                  binary_sensor.is_off: _ping_all
                then:
                  - switch.turn_on: _state_3
                else:
                  - switch.turn_on: _state_5
  - id: _state_5
    platform: lvgl
    widget: __state_5
    name: power cycle
    restore_mode: ALWAYS_OFF
    icon: mdi:power-cycle
    web_server:
      sorting_group_id: _state_group
      sorting_weight: 5
    on_turn_on:
      - script.execute:
          id: _enter
          state: 5
      - switch.turn_off: _power
      - lambda: !lambda id(_power_since)->set_when();
      - wait_until:
          condition:
            switch.is_off: _state_5
          timeout: 10s
      - if:
          condition:
            switch.is_on: _state_5
          then:
            - switch.turn_on: _power
            - lambda: !lambda id(_power_since)->set_when();
            - switch.turn_on: _state_1

display:
  - <<: *m5stack_cores3_display
    id: _display
    auto_clear_enabled: false
    update_interval: 1ms

touchscreen:
  - id: _touchscreen
    platform: _m5cores3_touchscreen
    update_interval: 50ms
    left:
      id: _brightness_decrement
      on_press:
        number.decrement:
          id: _brightness
          cycle: false
    center:
      id: _home
      on_press:
        lambda: |-
          auto tileview{id(_tileview)};
          lv_obj_set_tile(tileview, id(__state_tile), LV_ANIM_OFF);
          lv_event_send(tileview, LV_EVENT_VALUE_CHANGED, nullptr);
          id(_tile_iterator).reset();
    right:
      id: _brightness_increment
      on_press:
        number.increment:
          id: _brightness
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
define(mdi_network, \U000F06F3)dnl
define(mdi_network_off, \U000F0C9B)dnl
define(mdi_power, \U000F0425)dnl
define(mdi_power_cycle, \U000F0901)dnl
define(mdi_tag, \U000F04F9)dnl
font:
  - id: _font
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
          - "mdi_network"
          - "mdi_network_off"
          - "mdi_power"
          - "mdi_power_cycle"
          - "mdi_tag"
  - id: _font_small
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
          - "mdi_network"
          - "mdi_network_off"
          - "mdi_power"
          - "mdi_power_cycle"
          - "mdi_tag"
dnl
define(`__increment', `define(`$1', incr($1))')dnl

_format:

_since:
  - id: _boot_since
    name: since boot
    when: 0ns
    icon: mdi:arrow-up-bold-circle
    web_server:
      sorting_group_id: _uptime_group
      sorting_weight: 0
    on_value:
      lvgl.label.update:
        id: __boot_since
        text: !lambda return _format::duration(x);
  - id: _power_since
    name: since power cycle
    icon: mdi:power-cycle
    web_server:
      sorting_group_id: _power_group
      sorting_weight: 1
    on_value:
      lvgl.label.update:
        id: __power_since
        text: !lambda return _format::duration(x);

_ping:
  - none:
      id: _ping_none
      name: ping none
      icon: mdi:network-off
      web_server:
        sorting_group_id: _ping_summary_group
        sorting_weight: 0
      on_state:
        lvgl.widget.update:
          id: __ping_none
          state:
            checked: !lambda return x;
    some:
      id: _ping_some
      name: ping some
      icon: mdi:circle-half-full
      web_server:
        sorting_group_id: _ping_summary_group
        sorting_weight: 1
      on_state:
        lvgl.widget.update:
          id: __ping_some
          state:
            checked: !lambda return x;
    all:
      id: _ping_all
      name: ping all
      icon: mdi:network
      web_server:
        sorting_group_id: _ping_summary_group
        sorting_weight: 2
      on_state:
        lvgl.widget.update:
          id: __ping_all
          state:
            checked: !lambda return x;
    count:
      id: _ping_count
      name: ping count
      icon: mdi:pound
      web_server:
        sorting_group_id: _ping_summary_group
        sorting_weight: 3
      on_value:
        lvgl.label.update:
          id: __ping_some_label
          text: !lambda return to_string(static_cast<int>(x));
    since:
      id: _ping_since
      name: ping since
      icon: mdi:check-network
      web_server:
        sorting_group_id: _ping_summary_group
        sorting_weight: 4
      on_value:
        lvgl.label.update:
          id: __ping_since
          text: !lambda return _format::duration(x);
    targets:
define(`__count', `-1')dnl
define(host, `__increment(`__count')dnl
      - id: _ping_`'__count
        name: ping __count ($1 $2)
        address: $1
        interval: 10s
        timeout: 2s
        icon: mdi:cog
        web_server:
          sorting_group_id: _ping_target_group
          sorting_weight: __count
        on_state:
          - lvgl.widget.update:
              id: __ping_`'__count
              state:
                checked: !lambda return x;
          - lvgl.label.update:
              id: __ping_`'__count`'_label
              text: !lambda |-
                return std::string{
                  x ? "mdi_cog" : "mdi_cog_stop"};
        able:
          id: _ping_`'__count`'_able
          name: ping __count able
          icon: mdi:network
          web_server:
            sorting_group_id: _ping_target_group
            sorting_weight: __count
          on_state:
            - lvgl.widget.update:
                id: __ping_`'__count`'_able
                state:
                  checked: !lambda return x;
            - lvgl.label.update:
                id: __ping_`'__count`'_label_able
                text: !lambda |-
                  return std::string{
                    x ? "mdi_network" : "mdi_network_off"};
        since:
          id: _ping_`'__count`'_since
          name: ping __count since
          icon: mdi:check-network
          web_server:
            sorting_group_id: _ping_target_group
            sorting_weight: __count
          on_value:
            lvgl.label.update:
              id: __ping_`'__count`'_since
              text: !lambda return _format::duration(x);')dnl
include(hosts.m4)dnl
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
  displays: [_display]
  touchscreens:
    - _touchscreen
  theme:
    obj:
      bg_color: _bg_off
      border_width: 0
      radius: 0
      pad_all: 0
      text_font: _font
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
    - id: __label_style
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
                        id: _tileview
                        width: 100%
                        height: 100%
                        bg_color: _bg_off
                        tiles:
                          - id: __state_tile
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
                                  id: __state_1
                                  grid_cell_column_pos: 0
                                  grid_cell_column_span: 3
                                  grid_cell_row_pos: 0
                                  checkable: true
                                  widgets:
                                    - label:
                                        text: "mdi_cog_pause`'mdi_network_off"
                              - button:
                                  id: __state_2
                                  grid_cell_column_pos: 0
                                  grid_cell_column_span: 3
                                  grid_cell_row_pos: 0
                                  checkable: true
                                  hidden: true
                                  widgets:
                                    - label:
                                        text: "mdi_cog_pause`'mdi_network_off`'mdi_dots_horizontal"
                              - button:
                                  id: __state_3
                                  grid_cell_column_pos: 3
                                  grid_cell_column_span: 3
                                  grid_cell_row_pos: 0
                                  checkable: true
                                  widgets:
                                    - label:
                                        text: "mdi_cog_pause`'mdi_network"
                              - button:
                                  id: __state_4
                                  grid_cell_column_pos: 3
                                  grid_cell_column_span: 3
                                  grid_cell_row_pos: 0
                                  checkable: true
                                  hidden: true
                                  widgets:
                                    - label:
                                        text: "mdi_cog_pause`'mdi_network`'mdi_dots_horizontal"
                              - button:
                                  id: __state_0
                                  grid_cell_column_pos: 0
                                  grid_cell_column_span: 2
                                  grid_cell_row_pos: 1
                                  checkable: true
                                  widgets:
                                    - label:
                                        id: __state_0_label
                                        text: "mdi_cog"
                                  on_value:
                                    lvgl.label.update:
                                      id: __state_0_label
                                      text: !lambda |-
                                        return std::string{
                                          x ? "mdi_cog_stop" : "mdi_cog"};
                              - button:
                                  id: __power
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
                                        - switch.turn_on: _power
                                      else:
                                        - switch.turn_off: _power
                              - button:
                                  id: __state_5
                                  grid_cell_column_pos: 4
                                  grid_cell_column_span: 2
                                  grid_cell_row_pos: 1
                                  checkable: true
                                  widgets:
                                    - label:
                                        text: "mdi_power_cycle"
                              - button:
                                  id: __ping_none
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
                                  id: __ping_some
                                  grid_cell_column_pos: 2
                                  grid_cell_column_span: 2
                                  grid_cell_row_pos: 2
                                  checkable: true
                                  clickable: false
                                  checked:
                                    border_color: _fg_off
                                  widgets:
                                    - label:
                                        id: __ping_some_label
                                        text: "0"
                              - button:
                                  id: __ping_all
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
                          - id: __since_tile
                            row: 0
                            column: 1
                            dir: HOR
                            layout:
                              type: GRID
                              grid_columns: [FR(1), FR(5)]
                              grid_rows: [FR(1), FR(1), FR(1)]
                            widgets:
                              - label:
                                  styles: __label_style
                                  grid_cell_column_pos: 0
                                  grid_cell_row_pos: 0
                                  text: "mdi_check_network"
                              - label:
                                  styles: __label_style
                                  id: __ping_since
                                  grid_cell_column_pos: 1
                                  grid_cell_row_pos: 0
                                  text: "N/A"
                              - label:
                                  styles: __label_style
                                  grid_cell_column_pos: 0
                                  grid_cell_row_pos: 1
                                  text: "mdi_power_cycle"
                              - label:
                                  styles: __label_style
                                  id: __power_since
                                  grid_cell_column_pos: 1
                                  grid_cell_row_pos: 1
                                  text: "N/A"
                              - label:
                                  styles: __label_style
                                  grid_cell_column_pos: 0
                                  grid_cell_row_pos: 2
                                  text: "mdi_arrow_up_bold_circle"
                              - label:
                                  styles: __label_style
                                  id: __boot_since
                                  grid_cell_column_pos: 1
                                  grid_cell_row_pos: 2
                                  text: "N/A"
define(`__count', `-1')dnl
define(host, `__increment(`__count')dnl
                          - id: __ping_`'__count`'_tile
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
                              grid_rows: [FR(1), FR(1), FR(1)]
                            widgets:
                              - button:
                                  id: __ping_`'__count
                                  grid_cell_column_pos: 0
                                  grid_cell_row_pos: 0
                                  grid_cell_column_span: 3
                                  checkable: true
                                  widgets:
                                    - label:
                                        id: __ping_`'__count`'_label
                                        text: "mdi_cog_stop"
                                  on_value:
                                    switch.control:
                                      id: _ping_`'__count
                                      state: !lambda return x;
                              - button:
                                  id: __ping_`'__count`'_able
                                  grid_cell_column_pos: 3
                                  grid_cell_row_pos: 0
                                  grid_cell_column_span: 3
                                  checkable: true
                                  widgets:
                                    - label:
                                        id: __ping_`'__count`'_label_able
                                        text: "mdi_network_off"
                              - label:
                                  styles: __label_style
                                  grid_cell_column_pos: 0
                                  grid_cell_row_pos: 1
                                  text: "mdi_check_network"
                              - label:
                                  styles: __label_style
                                  id: __ping_`'__count`'_since
                                  grid_cell_column_pos: 1
                                  grid_cell_row_pos: 1
                                  grid_cell_column_span: 5
                                  text: "N/A"
                              - label:
                                  styles: __label_style
                                  grid_cell_column_pos: 0
                                  grid_cell_row_pos: 2
                                  text: "mdi_tag"
                              - label:
                                  styles: __label_style
                                  grid_cell_column_pos: 1
                                  grid_cell_row_pos: 2
                                  grid_cell_column_span: 5
                                  text: "$1 $2"')dnl
include(hosts.m4)dnl
undefine(`host')dnl
undefine(`__count')dnl
                    - button:
                        id: __unlock_overlay
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
                              text_font: _font_small
                              text: "mdi_arrow_left_bold"
                        on_click:
                          lambda: |-
                            auto tileview{id(_tileview)};
                            auto it{id(_tile_iterator)};
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
                              id: __unlock_label
                              text_font: _font_small
                              text: "mdi_lock"
                        on_value:
                          - lvgl.widget.update:
                              id: __unlock_overlay
                              hidden: !lambda return x;
                          - lvgl.label.update:
                              id: __unlock_label
                              text: !lambda |-
                                return std::string{
                                  x ? "mdi_lock_open" : "mdi_lock"};
                    - button:
                        flex_grow: 1
                        height: 100%
                        widgets:
                          - label:
                              text_font: _font_small
                              text: "mdi_arrow_right_bold"
                        on_click:
                          lambda: |-
                            auto tileview{id(_tileview)};
                            auto it{id(_tile_iterator)};
                            auto tile{reinterpret_cast<lv_obj_t *>(*++(*it))};
                            lv_obj_set_tile(tileview, tile, LV_ANIM_OFF);
                            lv_event_send(
                              tileview, LV_EVENT_VALUE_CHANGED, nullptr);

_rotation:
  - id: _tile_rotation
    items:
      - __state_tile
      - __since_tile
define(`__count', `-1')dnl
define(host, `__increment(`__count')dnl
      - __ping_`'__count`'_tile')dnl
include(hosts.m4)dnl
undefine(`host')dnl
undefine(`__count')dnl
    iterators:
      - id: _tile_iterator

number:
  - platform: template
    id: _brightness
    min_value: 0
    max_value: 255
    step: 32
    mode: slider
    optimistic: true
    restore_value: true
    initial_value: 127
    on_value:
      lambda: !lambda M5.Display.setBrightness(x);
