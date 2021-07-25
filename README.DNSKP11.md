# Hardware installation

This hardware installation guide assumes you already have a Panasonic DNSK-P11 installed and want to replace it. If you do not have a DNSK-P11 installed but your AC has the CN-WLAN connector you will have to find a fitting connector and route a cable from the mainboard yourself.

* Solder your ESP to your logic converter like this:

| **ESP32** | **Logic Converter**           | **AC connector** | **Notes** |
| --------- | ------------- | ---------------- | ----------- |
| 5V/VIN    | VCC/HV | 5V | Provides the ESP32 with power from the AC, make sure to connect it to the high voltage side if your logic converter has one |
| 3.3V      | VCC/LV | - | Connect the logic converter to 3.3V on the LV side |
| GND       | GND    | GND | Connect any ground from the ESP32 to the ground of the logic converter low voltage side |
| GPIO16    | LV1    | - | Connect GPIO16 to the first of your logic converter channels (Low voltage) |
| GPIO17    | LV2    | - | Connect GPIO17 to the second of your logic converter channels (Low voltage |
| -    | HV1    | RX | Connect the first of your logic converter channels to the AC RX pin (High voltage) |
| -    | HV2    | TX | Connect the second of your logic converter channels to the AX TX pin (High voltage) |

* Disconnect the AC mains supply
* Open up your AC according to the manual
* Your wifi adapter should be located on the left side of the unit like this:
![wifi adapter](images/wifi_module.jpg)
* Remove the old wifi adapter and unplug it
* Optional: While you can install the ESP where the old unit was I recommend rerouting the wire to the right side of the unit and placing the ESP where the external adapter sits. This makes it easier to replace in the future and doesn't require you to open your AC again
* Cut off the cable where the wifi connector sits:
![wifi adapter](images/connector.jpg)
Note: RX/TX is from the direction of the ESP, not the AC.
* Solder 4 jumper cables to the exposed wires
* Heat shrink the individual wires (**do not skip this step to avoid short ciruits**)
![wifi adapter](images/jumper_wires.jpg)
* Thread the now dangling wire through the front of the case
* Reinstall the front case of the AC as you would normally do
* Connect the jumper cables to your logic converter
* Place the ESP and the logic converter in the top right slot for the wifi adapter (you can use some velcro to fix it in place)
* Install the front cover and we are done

![wifi adapter](images/installed_controller.jpg)

Reconnect the AC mains supply and turn wifi back on. You should see a flashing wifi LED, followed by a faster flashing wifi LED and a solid wifi LED after about a minute. You can now connect this ESP to Home Assistant and control it from there.
