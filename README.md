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
