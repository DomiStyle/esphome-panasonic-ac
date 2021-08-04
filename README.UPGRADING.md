# Upgrading from 1.x to 2.x

ESPPAC 2.0 introduces support for the CZ-TACG1 wifi module but also comes with some breaking changes. In order to update your existing config to the new format you need to add the following:

## Add new switches

Support for eco mode and mild dry was added, you need to add the latter 2 sensors:

```
switch:
  - platform: template
    id: ac01_nanoex_switch
    name: ac01_nanoex_switch
    optimistic: true
  - platform: template
    id: ac01_eco_switch
    name: ac01_eco_switch
    optimistic: true
  - platform: template
    id: ac01_mild_dry_switch
    name: ac01_mild_dry_switch
    optimistic: true
```

## Specify which protocol to use

The previous `PanasonicAC` class has been replaced with `PanasonicACWLAN` and `PanasonicACCNT`. Choose the appropiate one, when upgrading from 1.x you want `PanasonicACWLAN`:

```
// For DNSK-P11
auto ac = new ESPPAC::WLAN::PanasonicACWLAN(id(ac_uart));
```

## Add new sensor callbacks

The new sensors need to be passed to the `PanasonicAC` class. Enable as appropiate for your AC:

```
// Enable as needed
// ac->set_nanoex_switch(id(ac01_nanoex_switch));
// ac->set_eco_switch(id(ac01_eco_switch));
// ac->set_mild_dry_switch(id(ac01_mild_dry_switch));
```

## Replace input_selects with select

The text sensors for manual tilt selection have been replaced with the new select element.

Remove the entire `text_sensor` block from your config and add the new select integration:

```
  select:
    - platform: template
      id: ac01_vertical_swing
      name: ac01_vertical_swing
      optimistic: true
      options:
        - up
        - up_center
        - center
        - down_center
        - down
    - platform: template
      id: ac01_horizontal_swing
      name: ac01_horizontal_swing
      optimistic: true
      options:
        - left
        - left_center
        - center
        - right_center
        - right
```

The functions `set_vertical_swing_sensor` and `set_horizontal_swing_sensor` have been renamed:

```
  ac->set_vertical_swing_select(id(ac_cnt_vertical_swing));
  ac->set_horizontal_swing_select(id(ac_cnt_horizontal_swing));
```

## Update automations

In case you added any automations to Home Assistant you might have to update them as powerful and quiet have moved to the preset and fan modes have been renamed.

Also, all previously added `input_select` are not used anymore and all automations now have to point to the new `select` elements.
