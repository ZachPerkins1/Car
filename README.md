# Car

## About
This project is my adventure into making a remote control car controllable from a computer along with a a little video stream so that you can see where you're driving. It's probably the most full stack project I've ever worked on seeing as I did everything from the GUI right down to the hardware level.

## The hardware
The hardware itself consists of:
- A remote control car that I ripped the body off
- A motor driver to control the 2 DC Motors
- An Arduino Uno to control the motor driver over PWM and act as a VRM for the raspi
- A Raspberry Pi Zero to communicate with the host machine and send signals to the Arduino over a serial line
- The client computer to send and receive commands

## How it works
The project is split into 3 subsystems: The vision, the controls, and the communications (which basically communicates statuses for the other 2 subsystems). All of them run in different threads.

### Vision
First, video is captured on the raspi zero and encoded to jpeg with about 50% loss. It is then sent over when requested by the client in chunks of 4096 bytes over a UDP socket until the whole image has been sent or a long enough time has passed that we can assume loss. This was one of the hardest parts to get working consistently

### Controls
Controls are captured from the keyboard by the client machine, encoded in json, then sent over to the car via TCP when it polls for them.

### Status
Status is captured by the car for each subsystem and stored in a thread safe class until the client computer requests them (this subsystem reverses the client-server model for the car and the computer). At this point the data is encoded in JSON and send for the client to decode and display on one of the status indicators

## Where it's at
This project is about 50% complete. I still need to
- Finish the C++ code so that it runs in multiple threads and has failsafes for all of the communication errors
- Implement the code in the Arduino to drive the motors as well as the serial communications
- Clean things up a bit to look less messy

## Progress
Right now this isn't my main focus but I plan to come back to it some time soon
