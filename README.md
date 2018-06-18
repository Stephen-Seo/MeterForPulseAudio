# Meter for PulseAudio

## Usage

Run `./MeterForPulseAudio -h` to print usage.

## Compiling

Run `git submodule update --init --recursive` first to get submodules required
for this project.

Other dependencies for this project are SFML 2 and PulseAudio.

Create a build directory to build in:  
`mkdir buildRelease; cd buildRelease`

Generate the Makefile:  
`cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr ..`

And build:  
`make`

Optionally install:  
`make DESTDIR=installToThisDir install`
