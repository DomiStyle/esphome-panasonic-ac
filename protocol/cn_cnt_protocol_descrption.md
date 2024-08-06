# CN-CNT protocol description

Source: https://github.com/DomiStyle/esphome-panasonic-ac/tree/master/protocol/cztacg1

## Data Types

### Modes

| Mode      | Value |
| --------- | ----- |
| Heat/Cool | 04    |
| Cool      | 34    |
| Heat      | 44    |
| Dry       | 24    |
| Fan only  | 64    |
| Off       | 30    |

### Fan Speeds

| Speed | Value |
| ----- | ----- |
| Auto  | A0    |
| 1     | 30    |
| 2     | 40    |
| 3     | 50    |
| 4     | 60    |
| 5     | 70    |

### Swing Mode

| Swing mode  | auto | left | left_center | center | right_center | right |
| ----------- | ---- | ---- | ----------- | ------ | ------------ | ----- |
| auto        | FD   | F9   | FA          | F6     | FB           | FC    |
| up          | 1D   | 19   | 1A          | 16     | 1B           | 1C    |
| up_center   | 2D   | 29   | 2A          | 26     | 2B           | 2C    |
| center      | 3D   | 39   | 3A          | 36     | 3B           | 3C    |
| down_center | 4D   | 49   | 4A          | 46     | 4B           | 4C    |
| down        | 5D   | 59   | 5A          | 56     | 5B           | 5C    |

### Presets

| Preset          | Value |
| --------------- | ----- |
| Normal          | 00    |
| Powerful        | 02    |
| Quiet           | 04    |
| Normal+nanoex   | 40    |
| Powerful+nanoex | 42    |
| Quiet+nanoex    | 44    |
| Econavi         | 10    |

### Mild Dry

| Mode | Value |
| ---- | ----- |
| Off  | 80    |
| On   | 7F    |

### Eco Mode

| Mode | Value |
| ---- | ----- |
| Off  | 00    |
| On   | 40    |


## Controller to AC status query

| ID | Value | Descrption    |
| -- | ----- |  ------------ |
| 0  | 70    | Header        |
| 1  | 0A    | Packet length |
| 2  | 00    |               |
| 3  | 00    |               |
| 4  | 00    |               |
| 5  | 00    |               |
| 6  | 00    |               |
| 7  | 00    |               |
| 8  | 00    |               |
| 9  | 00    |               |
| 10 | 00    |               |
| 11 | 00    |               |
| 12 | 86    | Checksum      |

## AC to controller status response

| ID | Value | Descrption                    | Comment                   |
| -- | ----- | ----------------------------- | ------------------------- |
| 0  | 70    | Header                        |                           |
| 1  | 20    | Packet length                 |                           |
| 2  | 34    | Mode                          |                           |
| 3  | 2B    | Target temperature \* 2       |                           |
| 4  | 80    | Mild dry                      | Maybe something else too? |
| 5  | 50    | Fan speed                     |                           |
| 6  | 19    | Horizontal & vertical swing   | 0 = not supported?        |
| 7  | 40    | Powerful/Quiet/nanoex/Econavi |                           |
| 8  | 00    | ?                             |                           |
| 9  | 40    | ?                             |                           |
| 10 | 00    | Eco mode                      |                           |
| 11 | 00    | ?                             |                           |
| 12 | 3E    | ?                             |                           |
| 13 | 2D    | ?                             |                           |
| 14 | 00    | ?                             |                           |
| 15 | 00    | ?                             |                           |
| 16 | 20    | ?                             |                           |
| 17 | 85    | ?                             |                           |
| 18 | 16    | Current temperature           |                           |
| 19 | 19    | Outside temperature           |                           |
| 20 | FF    | Marker                        |                           |
| 21 | 16    | Current temperature           |                           |
| 22 | 19    | Outside temperature           |                           |
| 23 | FF    | Marker                        |                           |
| 24 | 80    | ?                             |                           |
| 25 | 80    | ?                             |                           |
| 26 | FF    | Marker                        |                           |
| 27 | 80    | ?                             |                           |
| 28 | 47    | ?                             |                           |
| 29 | 02    | ?                             |                           |
| 30 | 10    | ?                             |                           |
| 31 | 80    | Changes?                      |                           |
| 32 | 03    | Changes?                      |                           |
| 33 | 55    | Changes?                      |                           |
| 34 | 8C    | Checksum                      |                           |