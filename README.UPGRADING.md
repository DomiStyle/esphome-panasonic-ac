# Upgrading from 1.x to 2.x

v2.0 introduces support for the CZ-TACG1 wifi module but also comes with many breaking changes. In order to update your existing config you will have to move your config over to the new external component format.

The climate entity and all sensors and switches are now automatically exposed to Home Assistant.
Take a look at [the new example file](ac.yaml.example) to see what has changed.

## Cleanup

If you previously pulled the old version of this repository you may remove the `esphome-panasonic-ac` folder, the repo gets pulled automatically now.

## Move from input_selects to select

If you previously added any input_select entities in Home Assistant to control the manual swing position you may remove them. They are not used anymore.

## Update automations

In case you added any automations to Home Assistant you might have to update them as powerful and quiet have moved to the preset and fan modes have been renamed.

Also, all previously added `input_select` are not used anymore and all automations now have to point to the new `select` elements.
