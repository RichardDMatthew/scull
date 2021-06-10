# scull

## code origins
The code in this repository was originally written as part of a group of simple drivers used for teaching different parts of driver development on a linux kernel.
The book this code was written for is quite popular but out of date: Linux Device Drivers Third Edition which is now available free at https://lwn.net/Kernel/LDD3/ and at https://www.oreilly.com/library/view/linux-device-drivers/0596005903/
The original code was developed for kernel 2.6 and had fallen out of date with changes to the linux kernel interface.
However the code was updated to work with more current kernels and placed in a git repository at git://github.com/martinezjavier/ldd3.git
The code compiles and appears to work with kernel 5.8, I've done minmum testing so far.

## building
**make** will do it all - except the sculltest.c

**make sculltest** will take care of that

## loading and unloading
**sudo ./scull_load**

**sudo ./scull_unload**

## testing
obviously **sudo ./sculltest**

### testing needs to be added to test more aspects of the driver
Most of the userland interface to the driver isn't tested in included test file.
I am adding more testing to exercise the rest of the driver

