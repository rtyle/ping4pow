# ping4pow

## Motivation

Upon network recovery,
sensitive equipment may need to be rebooted after things have settled.
For many such, power to the device can be cycled to achieve this.
Without automation, this must be done manually.
It would be better with automation that could recognize such conditions and cycle power.

There are commercially available/configurable devices that address this issue.

* [WattBox](https://www.snapav.com/shop/en/snapav/wattbox)
* [Ping Watchdog](https://www.netio-products.com/en/glossary/watchdog-ping-ip-watchdog)

This is a better solution.

## Requirements

* Small, attractive package
* Display with touchscreen user interface
* Home Assistant integrable but not required for automation
* Wired (ethernet) access for reliable network monitoring
* Powered by [Power-over-Ethernet](https://en.wikipedia.org/wiki/Power_over_Ethernet) (PoE)
* Inserted in the low voltage power cord of sensitive equipment
* Galvanically isolated switching of equipment power
* Network health is determined by [ping](https://en.wikipedia.org/wiki/Ping_(networking_utility))ing multiple IP addresses
* Network failure is when all addresses can not be pinged.
* Network recovery is when all addresses can be pinged steadily.
* Upon network recovery, power is cycled.

## Architecture

### Hardware

[M5Stack](https://docs.m5stack.com/en/start/intro/intro) products are used to provide a small and attractive hardware solution
that does not require a custom enclosure for switching low voltage loads.
Our stack needs a core with support from a base LAN PoE module.

* [CoreS3-SE](https://docs.m5stack.com/en/core/M5CoreS3%20SE)
* [Base LAN PoE v1.2](https://docs.m5stack.com/en/base/lan_poe_v12)

To support both the core display and the ethernet LAN at the same time, a custom M5-Bus adapter must be used to resolve GPIO pin conflicts. This might be done using an M5Bus wire-bodged Module Proto. Alternately, the PCB of the Module Proto might be swapped out with one that has the bodge baked in (see [adapter](kicad/projects/adapter)).

* [Module Proto](https://docs.m5stack.com/en/module/proto)

To switch the load we will need a relay. M5Stack provides a 4Relay Module. Alternately, the M5-Bus adapter might be expanded to provide a relay using the frame of an M5Stack Module13.2 Proto (see [adapter_relay](kicad/projects/adapter_relay)).

* [Module13.2 4Relay v1.1](https://docs.m5stack.com/en/module/4Relay%20Module%2013.2_V1.1)
* [Module13.2 Proto](https://docs.m5stack.com/en/module/proto13.2)

### Software

[ESPHome](https://esphome.io/) is used because of its ability to support our chosen hardware and the potential for other choices.
It can be configured in a high level language (YAML) and will only do what you tell it to do.
A configured device can perform all the automations without any external support.
Optionally, such a device can be used by Home Assistant.

## Deployment

### Hardware

The stack should be assembled with the core on top, followed by the relay solution, the adapter solution and finally the base.

#### Module13.2 Proto adapter_relay

This solution uses a Module13.2 Proto frame with the PCB replaced with one built from the [adapter_relay](kicad/projects/adapter_relay) kicad project. There is no need for a separate M5-Bus adapter.

The adapter_relay uses a double-pole double-throw (DPDT) [signal relay](https://na.industrial.panasonic.com/products/relays-contactors/mechanical-signal-relays/lineup/signal-relays/series/119572/model/119942) to switch both conductors of the galvanically isolated DC load. Jumpers must be installed in the normally-closed (NC) positions. The load is passed through the board using standard 5.5mm x 2.1mm DC barrel connections. Openings in the frame of the Module13.2 Proto must be expanded to accomodate the DC jacks. It doesn't matter which jack is used for which plug.

#### Module13.2 4Relay v1.1 and Module Proto adapter

This solution uses the Module13.2 4Relay v1.1 and a separate M5-Bus Module Proto based adapter. The adapter can be built from the [adapter](kicad/projects/adapter) kicad project or hand made using its schematic for a reference. If hand made, the disconnected male pins should be clipped and the re-routing of signals made using bodge wires.

<img src=docs/adapter.schematic.png class=dark>

Only the first [signal relay](https://www.amazon.com/gp/product/B0CWCTHCFT) in the module will be used.
It will be configured to provide module input power to relay output power in a Normally Closed (NC) fashion.
We want the relay to operate in a NC fashion so that when power is removed from the stack the relay will be closed
(power to the controlled device will be on).

Module input power must be galvanically isolated from the M5-Bus (stack) HPWR and system GND.
**Remove** the jumpers that connect these.

![](docs/relay.hpwr.png)

Set the relay jumpers in the "load" configuration.

![](docs/relay.load.webp)

The relay is hard-wired on the module in a Normally Open (NO) fashion.
That is, the relay output power is connected by PCB trace to the NO pin on the relay.
Cut this trace and add a bodge wire to connect the relay ouput power with the NC pin on the relay.

<img src=docs/relay.png class=dark>

Wire module input power pins to an adapter pigtail that accepts the plug of the power adapter of the controlled device.
Alternately, cut the plug off the power adapter and wire the adapter directly to the 4relay input power plug.
Wire relay power output pins to an adapter pigtail that provides the power plug expected by the controlled device.
Alternately, cut the plug off the power adapter and wire the plug directly to the 4relay output power plug.

#### Base LAN PoE v1.2

Stack an M5-Bus adapter on top using one of the methods described above.
Power, and supply network access, over the RJ45 ethernet port.

### Software

These instructions are written for a Fedora Linux platform.
Similar steps may be taken for other Linux, MacOS or Windows platforms.

Get the source from this repository by the appropriate method:

    git clone https://github.com/rtyle/ping4pow.git
    git clone git@github.com:rtyle/ping4pow.git

All commands documented here are executed from this directory.

    cd ping4pow

Update all submodules of this repository.

    git submodule update --init --recursive

Install python

    sudo dnf install python

Create a python virtual environment for this project.

    python -m venv .venv

Activate the virtual environment every time you want to use ESPHome within this project.

    source .venv/bin/activate

With your virtual environment activated, install ESPHome using pip:

    pip install esphome

Configure the order, address and name of each host to be ping-monitored.
Each is specifed by an address, name pair in an m4 host macro invocation.

    vi config/hosts.m4

Connect, by USB cable, your computer with the M5Stack CoreS3.
Build, install and run firmware.

    m4 config/ping4pow.m4 > config/ping4pow.yaml
    esphome run config/ping4pow.yaml

Connect by ethernet to a power-over-ethernet capable port and unplug the USB cable. Connect the switched load to the relay.

## Usage

Each ping target defined (by address & name) and ordered (#) in the `hosts.m4` file will be periodically pinged.
Each will be represented by a

* `ping # (address name)` switch (where # is its order) to enable/disable pinging and summary consideration of the target.
* `ping # able` binary sensor that reflects the pingability of the target.
* `ping # since` sensor that reflects the time elapsed since the last change in pingability.

These target ping sensors will be summarized by a

* `ping none` binary sensor that is true unless an enabled target is pingable.
* `ping all` binary sensor that is true unless an enabled target is not pingable.
* `ping some` binary sensor that is true if "ping none" and "ping all" are false.
* `ping count` sensor that reflects the number of enabled and pingable targets.
* `ping since` sensor that reflects the time elapsed since the last change in target pingability.

The automation provided by ping4pow is implemented as a state machine.
These states are reflected in the user interface as switches.
Only one state/switch can be active/on at a time.
States can be entered through a switch action in the user interface.
These states/switches are:

* `0. Stop`. When on, the state machine is stopped. Turn off to advance to state 1.
* `1. Wait for ping none`. Then advance to state 2.
* `2. Wait for ping all`. Then advance to state 3.
* `3. Wait while ping all holds steady`. Then advance to state 4; otherwise, retreat to state 2.
* `4. Power cycle`. Turn power off, pause, turn power on and then start over in state 1.

In addition, ping4pow exposes

* A `power` switch that reflects/controls power to the connected load.
* A `since boot` sensor that reflects the time elapsed since booting the ping4pow device.
* A `since power cycle` sensor that reflects the time elapsed since the power to the load was automatically affected.

All since sensors are in units of seconds.

A web-based user interface at http://ping4pow.local exposes these switches and sensors.

These are also presented locally on the ping4pow device's display.

Under the display is a labeled bezel.
<picture>
<source media="(prefers-color-scheme: dark)" srcset="docs/mdi/chevron-double-left.dark.svg">
<source media="(prefers-color-scheme: light)" srcset="docs/mdi/chevron-double-left.light.svg">
<img src="docs/mdi/chevron-double-left.light.svg">
</picture><picture>
<source media="(prefers-color-scheme: dark)" srcset="docs/mdi/circle-outline.dark.svg">
<source media="(prefers-color-scheme: light)" srcset="docs/mdi/circle-outline.light.svg">
<img src="docs/mdi/circle-outline.light.svg">
</picture><picture>
<source media="(prefers-color-scheme: dark)" srcset="docs/mdi/chevron-double-right.dark.svg">
<source media="(prefers-color-scheme: light)" srcset="docs/mdi/chevron-double-right.light.svg">
<img src="docs/mdi/chevron-double-right.light.svg">
</picture>

<table style="text-align: center;">
  <tr>
    <td><img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/chevron-double-left.svg" class="icon"></td>
    <td><img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/circle-outline.svg" class="icon"></td>
    <td><img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/chevron-double-right.svg" class="icon"></td>
  </tr>
</table>

The following actions may be taken by touching the labels.

* <img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/chevron-double-left.svg" class="icon"> Decrease display brightness
* <img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/chevron-double-right.svg" class="icon"> Increase display brightness

At the bottom of the display is a user interface lock and tile navigation buttons

<table style="text-align: center;">
  <tr>
    <td><img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/arrow-left-bold.svg" class="icon"></td>
    <td><img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/lock.svg" class="icon"></td>
    <td><img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/arrow-right-bold.svg" class="icon"></td>
  </tr>
</table>

* <img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/arrow-left-bold.svg" class="icon"> Previous tile
* <img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/lock.svg" class="icon"> User input is locked (<img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/lock-open.svg" class="icon"> when not)
* <img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/arrow-right-bold.svg" class="icon"> Next tile

The rest of the display is used for the current tile.
Using the next and previous tile controls, one can cycle through them.
The tiles are:

* State tile
* Since tile
* Target tile (one for each)

### State Tile

<table style="text-align: center;">
<tr>
<td><img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/cog-pause.svg" class="icon"><img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/network-off.svg" class="icon"></td>
<td><img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/cog-pause.svg" class="icon"><img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/network.svg" class="icon"></td>
<td><img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/cog-pause.svg" class="icon"><img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/network.svg" class="icon"><img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/dots-horizontal.svg" class="icon"></td>
</tr>
<tr>
<td><img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/cog.svg" class="icon"></td>
<td><img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/power.svg" class="icon"></td>
<td><img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/power-cycle.svg" class="icon"></td>
</tr>
<tr>
<td><img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/network-off.svg" class="icon"></td>
<td><img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/pound.svg" class="icon"></td>
<td><img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/network.svg" class="icon"></td>
</tr>
</table>

The top row of buttons reflect states 1, 2 and 3.

* <img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/cog-pause.svg" class="icon"><img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/network-off.svg" class="icon"> `1. Wait for ping none`
* <img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/cog-pause.svg" class="icon"><img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/network.svg" class="icon"> `2. Wait for ping all`
* <img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/cog-pause.svg" class="icon"><img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/network.svg" class="icon"><img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/dots-horizontal.svg" class="icon"> `3. Wait while ping all holds steady`

The middle row of buttons reflect state 0, power and state 4

* <img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/cog.svg" class="icon"> `0. Stop` (<img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/cog-stop.svg" class="icon"> when stopped)
* <img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/power.svg" class="icon"> `Power`
* <img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/power-cycle.svg" class="icon"> `4. Power cycle`

The bottom row reflects the summary ping sensors. Only one of these will be on. The label on the widget reflecting state of the `ping some` binary sensor will reflect the `ping count` sensor value regardless.

* <img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/network-off.svg" class="icon"> `ping none`
* <img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/pound.svg" class="icon"> `ping some` and `ping count` (#)
* <img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/network.svg" class="icon"> `ping all`

### Since Tile

The three rows of this tile reflect times since an event.
This time is formatted as either "N/A" (if there was never such an event) or the number of days, hours, minutes and seconds (D HH:MM:SS) since the event.

* <img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/check-network.svg" class="icon"> `ping since`
* <img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/power-cycle.svg" class="icon"> `since power cycle`
* <img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/arrow-up-bold-circle.svg" class="icon"> `since boot`

### Target Tile

There is a target tile for each ping target in `hosts.m4`.

The first row of this tile reflects whether the target is enabled and its pingability.

* <img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/cog.svg" class="icon"> `ping # (address name)` enabled (<img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/cog-stop.svg" class="icon"> when not)
* <img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/network.svg" class="icon"> `ping # able` (<img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/network-off.svg" class="icon"> when not)

The next two rows are for `ping # since` and for identifying the target by address and name.

* <img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/check-network.svg" class="icon"> `ping # since`
* <img src="https://cdn.jsdelivr.net/npm/@mdi/svg/svg/tag.svg" class="icon"> `address` `name`
