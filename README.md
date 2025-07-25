# ping4pow

## Motivation

Upon network recovery (from a power failure or an update to network infrastruture)
devices may need to be forced to reboot after things have settled.
For many devices, power to the device can be cycled to achieve this.
Do this automatically.

There are commercially available/configurable devices that address this issue.

* [WattBox](https://www.snapav.com/shop/en/snapav/wattbox)
* [Ping Watchdog](https://www.netio-products.com/en/glossary/watchdog-ping-ip-watchdog)

This is another solution.

## Architecture

### Hardware

[M5Stack](https://docs.m5stack.com/en/start/intro/intro) products are used to provide a small and attractive hardware solution
that does not require a custom enclosure for switching low voltage loads.
Our stack needs a core with support from relay and PoE modules.

* [CoreS3-SE](https://docs.m5stack.com/en/core/M5CoreS3%20SE)
* [Module13.2 4Relay v1.1](https://docs.m5stack.com/en/module/4Relay%20Module%2013.2_V1.1)
* [Base LAN PoE v1.2](https://docs.m5stack.com/en/base/lan_poe_v12)

### Software

[ESPHome](https://esphome.io/) is used because of its ability to support our chosen hardware and the potential for other choices.
It can be configured in a high level language (YAML) and will only do what you tell it to do.
A configured device can perform all the automations without any external support.
Optionally, such a device can be used by Home Assistant.

## Deployment

### Hardware

The stack will be assembled with the PoE module on the bottom, the relay module in the middle and the core on the top.

#### Relay Module

Only the first relay in the module will be used.
It will be configured to provide module input power to relay output power in a Normally Closed (NC) fashion.
We want the relay to operate in a NC fashion so that when power is removed from the stack the relay will be closed
(power to the controlled device will be on).

Remove module input power jumpers to galvanically isolate these pins from the M5-Bus (stack) HPWR and system GND.
Add a jumper such that the module input power pin 1 is connected to the relay output power pin 1.
Add a jumper such that the module input power pin 2 is connected to the relay common pin.

The relay is hard-wired on the module in a Normally Open (NO) fashion.
That is, the relay output power pin 2 is connected by PCB trace to the NO pin on the relay.
Cut this trace and add a bodge wire to connect the relay ouput power pin 2 with the NC pin on the relay.
See PCB board layout for this [relay](https://mktechnic.com/product/hk4100f-x-shg).

Wire module input power pins to an adapter pigtail that accepts the plug of the power adapter of the controlled device.
Wire relay power output pins to an adapter pigtail that provides the power plug expected by the controlled device.
Use pins 1 for the always-connected wire and pins 2 for the relay-switched wire.

#### PoE Module

Supply power to the stack and network connectivity through the RJ45 jack on the PoE module.

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

To copy or reinstall M5Stack demo firmware, download and extract M5Burner

    https://m5burner-cdn.m5stack.com/app/M5Burner-v3-beta-linux-x64.zip

Build, install and run firmware

    esphome run config/ping4pow.yaml

