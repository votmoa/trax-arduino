# T-RAX Arduino

This is the Arduino C++(ish) code for the RFO Roof Controller. It is complemented by the T-RAX Java code
which implements the user interface for the Roof Controller.

### Arduino Input/Output Pins

| Pin | Name          | Direction | Description |
|-----|---------------|-----------|-------------|
| 2   | weatherOK     | Input     | Boltwood "Weather OK" signal
| 3   | securityOK    | Input     | Signal from the Arduino based CCD security system
| 4   | bldgPowerIn   | Input     | Power detection for the buidling
| 5   | roofPowerIn   | Input     | Power detection for the roof
| 6   | mountPowerIn  | Input     | Power detection for the mount
| 7   | roofOpen      | Input     | Roof open position sensor
| 8   | roofClosed    | Input     | Roof closed position sensor
| 9   | mountParked   | Input     | Park position sensor for the mount
| 10  | roofPowerOut  | Output    | Roof Power enable
| 11  | mountPowerOut | Output    | Mount Power enable
| 12  | fobOutput     | Output    | Output to physical fob transmitter
| 13  | heartLed      | Output    | Onboard LED to indicate system status

### Serial I/O Commands

| Command     | Description |
|-------------|-------------|
| Stop        | Emergency Stop -- immediately turn off roof power
| RPon        | Roof Power On
| RPoff       | Roof Power Off
| MPon        | Mount Power On
| MPoff       | Mount Power Off
| Open        | Close Roof -- toggle fob ouptut if it's safe to close the roof
| Close       | Open Roof -- toggle fob output if it's safe to open the roof
| OverrideOn  | Turn on Emergency Override mode -- ignore safety tests and act on commands no matter what
| OverrideOff | Turn off Emergency Override mode -- return to normal safety tests
| DebugOn     | Turn on debugging messages
| DebugOff    | Turn off debugging messages
| Status      | Display sensoer status

### Function Descriptions

TBD
