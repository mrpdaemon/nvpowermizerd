nvpowermizerd
=============

nvpowermizerd is a daemon to improve the PowerMizer mode switching behavior for
nVidia cards on Linux/Unix. It uses the X idle time information to aggressively
switch to full power mode when the user is interacting with the system, and
gradually switch back to adaptive mode when the user is idle for a certain
amount of time.

## Why?

The main issue with nVidia's own PowerMizer "Adaptive" mode is that it's not
aggressive enough. It depends on GPU load to make mode switching decisions,
which results in low desktop performance (for example with gnome-shell) when
the PowerMizer is in its lowest setting. This results in choppy shell animations
etc. Even though PowerMizer eventually *does* switch to performance mode, it is
still not desirable to suffer choppy animations with such powerful GPU's - we
shouldn't have to run the GPU at full blast all the time to have smooth shell
animations.

## How

nvpowermizerd monitors the X idle time using the XScreenSaver extension, and
uses that information to toggle the PowerMizer mode between "Adaptive" and
"Full Performance". We switch to full performance mode very quickly (ex. 10ms)
after detecting an idle->active transition - and stay at full performance mode
until the user is idle for a long time (ex. 20 seconds). This results in smooth
GPU performance while the user is active, and power savings when the user is
idle.

## Usage

To compile npowermizerd, simply type:

    make

which will create the `nvpowermizerd` executable. You can then run this
executable directly, or add it to your X session's startup applications so
it is run automatically. To enable debugging logs compile with:

    CFLAGS="-DDEBUG" make
