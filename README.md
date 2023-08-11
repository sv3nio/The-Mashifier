# The Mashifier

Some time ago a friend gave me an old [Adafruit Feather 32u4 Bluefruit LE](https://www.adafruit.com/product/2829) controller. Given how outdated-but-still-useful the unit was, I decided to put it to good use and learn how to make a gaming controller for that old collection of ROMs I had kicking around. The result is this:

<img alt="The Mashifier" src="/img/the-mashifier-a.jpg" width=400><br/>

A super-responsive, easy to use BLE gaming controller for any Bluetooth compatible system. It's waaaay too much fun! :wink: (More photos in the `img` directory).

## Key Features

#### Brainless Pairing
Simply hold the START and SELECT buttons for 3 seconds while powering on. Connect to "The Mashifier". If at any point there's an issue with pairing, simply start over and unit will wipe the pairing history.

#### Crazy Responsive
The Mashifier is designed to do one thing very well: send commands to a gaming system/emulator. It also changes the LED colour based on battery level (but you'll be to busy playing to notice).

#### Combo Friendly
With support for up to 6 simultaneous button presses, The Mashifier can send complex commands for combo games like Street Fighter and Mortal Combat.

#### Awesome Battery Life
Of course, while this depends on how large of a battery you provide, The Mashifier is as minimal and effecient as possible to minimize charge time.

## Usage

This is not a beginner project, and while the sketch is adaptable to almost any Arduino-based BLE controller, you will need to ensure it has enough pins/memory to support at least 10 buttons (4 x joystick, 4 x main buttons, 2 x start & select). Decisions will need to be made on how to handle battery charging, etc. 3D print designs are on [Tinkercad](https://www.tinkercad.com/things/hxekYVrqBbI).
