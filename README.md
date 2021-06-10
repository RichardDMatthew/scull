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

# code
The code is very clean but built as a teaching tool. So there are parts that can taken out. Otherwise it's very easy to walk through.

## scull.init
### macros
#### SCULL_NR_DEVS
Set to 4, I'm not sure what they do with all 4 devices (files). There is a mention of the memory being global and persistent. /dev/scull0 - /dev/scull3 are all created (and removed) at the same time and have different memory buffers. Maybe one device can be used for writing data and another can be used for reading info about the results of operations, don't know how you want to use this

#### SCULL_QUANTUM 
The scull data buffer is a linked list of various allocations of n length. SCULL_QUANTUM is the size of the allocations used. I think this should be a multiple of 64 bits, 512 bytes or whatever.

#### SCULL_QSET 
I'm not clear on how this is used. I need to look at it more.
The size of the original linked list. It is set to 1000 at init time. This seems a lot larger than I expect is needed for the application. Of course allocating a buffer with enough size at init time prevents the need to allocate at runtime. I doubt if that much memory is needed.

#### SCULL_P_BUFFER 
There are versions of scull with pipes, this is the size of the circular buffer used

#### structs
a simple linked list struct and device struct - straight forward

#### prototypes
straight forward here as well

#### IOCTLs
you'll want to uses a unique magic number, from what I understand there is a file where current magic numbers are listed. I'll have to look it up unless you know where that is offhand.

All the IOCTL defs in this code are for reading and setting the size of the memory buffers both quantum and qset. They can be access with a pointer or with an int. Pick your flavor, but most of this can be removed.

### main.c
#### scull_init_module
It all looks good until I get down to to friend devices. 
##### dev += scull_p_init(dev);
This initializes a pipe buffer that I don't think is used in this flavor. I'm still looking around for why this is here.
#### scull_open
Open also trims the device's memory buffer.
#### scull_write
This allocates more memory as needed which can turn into a memory leak.
It returns how much was written or a negative number on fault.
It only writes up to the end of a quantum so it's good to have quantum and data size the same for efficiency.
#### scull_read
Like write it only reads to the end of a quantum
#### scull_ioctl
mostly get/set stuff for memory buffer size
At the end are a couple IOCTL's for the pipe buffer - again, not sure yet if this is used, still looking
#### scull_llseek
seems pretty useful if you want a separate write and read buffer area separated by an offset
I don't see any protections from seeking off the end of the data, maybe that's somewhere else
#### scull_cleanup
frees all the data for all the devices
#### scull_follow
utility function for following down the linked list
#### scull_seq_start
#### scull_seq_next
#### scull_seq_stop
#### scull_seq_show
these are all used as function pointers into a struct scull_seq_ops. These are related to sequencing through the different devices but I'm not sure how it's used or what it's for
#### scull_read_procmem
this is a debugging function. probably really useful. It walks through all four drivers


## Stuff I don't get yet or concerns
### why are they creating a pipe buffer or have IOCTLs for the pipe buffer
### scull can allocate memory until there isn't anymore
### need to see if it's possible to seek off the end of the data
### I don't have test cases for most of this stuff

