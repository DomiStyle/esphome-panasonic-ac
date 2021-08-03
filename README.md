# Overview

An open source alternative for the Panasonic wi-fi adapter that works locally without the cloud.

# Features

* Control your AC locally via Home Assistant, MQTT or directly
* Instantly control the AC without any delay like in the Comfort Cloud app
* Receive live reports and state from the AC
* Uses the UART interface on the AC instead of the IR interface
* Provides a drop-in replacement for the Panasonic DNSK-P11 and the CZ-TACG1 wifi module

# Supported hardware

This library works with both the CN-CNT port and the CN-WLAN port. CN-WLAN is only available on newer units.

Works on the ESP8266 but ESP32 is preferred for the multiple hardware serial ports.

# Requirements

* ESP32 (or ESP8266) ([supported by ESPHome](https://esphome.io/#devices))
* 5V to 3.3V bi-directional Logic Converter (minimum 2 channels, available as pre-soldered prototyping boards)
* Female-Female Jumper cables
* Soldering iron
* Wires to solder from Logic converter to ESP
* Heat shrink
* ESPHome 1.20.1 or newer
* Home Assistant 2021.8.0 or newer

# Notes

* **Make sure to disconnect mains power before opening your AC, the mains contacts are exposed and can be touched by accident!**
* **Do not connect your ESP32/ESP8266 directly to the AC, the AC uses 5V while the ESPs use 3.3V!**
* **While installation is fairly straightforward I do not take any responsibility for any damage done to you or your AC during installation**
* The DNSK-P11 and the CZ-TACG1 use different types of connectors, make sure to connect to the correct one

# Software installation

This software installation guide assumes some familiarity with ESPHome.

* Pull this repository
* Copy the `ac.yaml.example` one directory above and rename it to `ac.yaml` so your directory structure looks like this:
```
ac.yaml
esphome-panasonic-ac/
  esppac.cpp
  ...
```
* Uncomment one of the lines on the bottom of the file depending on your adapter type
* Adjust the `ac.yaml` to your needs
* Connect your ESP
* Run `esphome ac.yaml run` and choose your serial port (or do this via the Home Assistant UI)
* If you see the handshake messages being sent (DNSK-P11) or polling requests being sent (CZ-TACG1) in the log you are good to go
* Disconnect the ESP and continue with hardware installation

## Adding manual swing selection to Home Assistant

In order to manually adjust the swing modes for the AC, two input_select fields have to be added to Home Assistant manually.

Configuration -> Helpers -> Add -> Dropdown:

Name: Horizontal swing

Icon: mdi:swap-horizontal

Options:
* left
* left_center
* center
* right_center
* right


Name: Vertical swing

Icon: mdi:swap-vertical

Options:
* down
* down_center
* center
* up_center
* up

After that set the entity IDs of those dropdowns to the entity_id set in your `ac.yaml`:

```
  text_sensor:
    - platform: homeassistant
      id: ac01_vertical_swing
      name: ac01_vertical_swing
      entity_id: **input_select.ac01_vertical_swing**
      on_value:
        - homeassistant.service:
            service: input_select.select_option
            data_template:
              entity_id: **input_select.ac01_vertical_swing**
              option: "{{ swing }}"
            variables:
              swing: |-
                return id(ac01_vertical_swing).state;
```

## Setting supported features

Since Panasonic ACs support different features you can comment out the lines at the bottom of your `ac.yaml`:

```
  // Enable as needed
  // ac->set_nanoex_switch(id(ac01_nanoex_switch));
  // ac->set_eco_switch(id(ac01_eco_switch));
  // ac->set_mild_dry_switch(id(ac01_mild_dry_switch));
```

In order to find out which features are supported by your AC, check the remote that came with it.
To clean up the sensors in Home Assistant you can just delete them:

## Upgrading from 1.x to 2.x

[Upgrade instructions](README.UPGRADING.md)

# Hardware installation

[Hardware installation for DNSK-P11](README.DNSKP11.md)

[Hardware installation for CZ-TACG1](README.CZTACG1.md)
