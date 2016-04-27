# ubirch lights, a sensory installation of light color

It is the code that runs the [re:publica 2015 FEWL installation](https://www.facebook.com/findingeuropewithlights/). The
code runs on our own first generation boards insight hand made lamps as well as special RGB sensors
that run for a long time on battery and work world-wide.

![re:publica 2015 FEWL installation](https://scontent.ftxl1-1.fna.fbcdn.net/hphotos-xlt1/t31.0-8/11127043_928053903912202_2710786841221231166_o.jpg)

## Building

> This code is for the ```ubirch #1 r0.1```, Arduino compatible.

Expects an installation of the [Arduino IDE](https://www.arduino.cc/) + [AVR compiler](https://github.com/osx-cross/homebrew-avr/)
and [CMake](https://cmake.org/).

Please clone the repository recursively to ensure all submodules are also checked out:
```
git clone --recursive https://github.com/ubirch/ubirch-lights
```

Then build the code:
```
cd ubirch-lights
mkdir build
cd build
cmake ..
make
```

### RGB Sensor Code

The sensor POSTs measures the RGB values in 16 bit and sends them to the server. It will also do
some auto-compensation depending on the brightness of the sourroundings, changing the sensitivity/range
between 375 lux in darker environments and 10k lux in bright environments. This needs to be taken
into account when comparing color values.

```
{
  "v":"0.0.1",
  "a":"z3UuSIOGG0gPLpQchbBUliKmnVLS91SYbp7GScKf17hXBCen27tSeEQXoJ2YKE2Yb9IHbLU6Ctmy88/W3ImP0w==",
  "s":"NgtG1n1eorgEFXiuoDwIW6vuQ1956bROeIE4cRqLBbXDtaPtdP1UpFUPb+3NH5hC4XOm1ZjvxFQAueGn7QKrSA==",
  "p":{"r":21357,"g":14254,"b":11646,"s":0,"la":"52.505257","lo":"13.475882","ba":100,"lp":1,"e":0}
}
```

- ```v``` is the protocol version (```0.0.1```)
- ```a``` is the authorization key (hashed)
- ```s``` is a hash of the payload signature
- ```p``` the actual sensor payload
  - ```r```,```g```,```b``` are 16 bit color values (0-65535)
  - ```s``` is the sensitivity/range at which the colors were measured (```0``` = 375 lux or ```1``` = 10k lux)
  - ```la```,```lo``` is the current approximate geo-location of the sensor
  - ```ba``` is the current battery status (percent full, 0-100)
  - ```lp``` is the amount of loops without reboot
  - ```e``` is an error code bitfield which may contain:
    ```
    0b00000001 - RGB sensor failed (electrical or I2C error)
    0b00000010 - protocol mismatch in last response
    0b00000100 - signature of last response could not be verified
    0b00001000 - json parsing of last response failed (json syntax error?)
    0b10000000 - out of memory parsing last response (possibly due to too large response payload)
    0b01000000 - could not establish a mobile connection last time
    ```

In response the sensor expects the following:

```
{
  "v":"0.0.1",
  "s":"Z63ZeXEMXbWoIQLTTPWcArsVLt6ePXOHJG1rhE9QCrRJe2MhL9rZ5tSyEKK7h7Z6W07IFknzaiL84uKdUWjy4g==",
  "p":{"s":0,"ir":20,"i":900}
}
```

- ```v``` is the protocol version (currently accepted is ```0.0.x```)
- ```s``` is a hash of the payload signature
- ```p``` the actual sensor payload
  - ```s``` is the sensitivity it should by default measure with (```0``` = 375 lux or ```1``` = 10k lux)
  - ```ir``` the infrared filter setting (0 - 63, max is default)
  - ``i`` - the sleep interval

To debug the sensor, connect to the serial port (middle Grove) with ```115200 8N1```. It will
print some diagnostic output to identify a possible problem.


## LICENSE

    Copyright 2015 ubirch GmbH (http://www.ubirch.com)

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
