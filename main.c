/*
 * main.c -- the bare swaphints char module
 *
 * Copyright (C) 2001 Alessandro Rubini and Jonathan Corbet
 * Copyright (C) 2001 O'Reilly & Associates
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.  The citation
 * should list that the code comes from the book "Linux Device
 * Drivers" by Alessandro Rubini and Jonathan Corbet, published
 * by O'Reilly & Associates.   No warranty is attached;
 * we cannot take responsibility for errors or fitness for use.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <linux/seq_file.h>
#include <linux/cdev.h>

#include <linux/uaccess.h>	/* copy_*_user */

#include "swaphints.h"		/* local definitions */
#include "access_ok_version.h"
#include "proc_ops_version.h"

/*
 * Our parameters which can be set at load time.
 */

int swaphints_major =   SWAPHINTS_MAJOR;
int swaphints_minor =   0;
int swaphints_nr_devs = SWAPHINTS_NR_DEVS;	/* number of bare swaphints devices */

module_param(swaphints_major, int, S_IRUGO);
module_param(swaphints_minor, int, S_IRUGO);
module_param(swaphints_nr_devs, int, S_IRUGO);

MODULE_AUTHOR("Alessandro Rubini, Jonathan Corbet");
MODULE_LICENSE("Dual BSD/GPL");

struct swaphints_dev *swaphints_devices;	/* allocated in swaphints_init_module */
swaphints_request_t *requests;
swaphints_response_t *responses;


/*
 * Open and close
 */

int swaphints_open(struct inode *inode, struct file *filp)
{
	struct swaphints_dev *dev; /* device information */

	dev = container_of(inode->i_cdev, struct swaphints_dev, cdev);
	filp->private_data = dev; /* for other methods */

	return 0;          /* success */
}

int swaphints_release(struct inode *inode, struct file *filp)
{
	return 0;
}

/*
 * The ioctl() implementation
 */

long swaphints_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

	int err = 0;
	int retval = 0;
    
	/*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if (_IOC_TYPE(cmd) != SWAPHINTS_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > SWAPHINTS_IOC_MAXNR) return -ENOTTY;

	/*
	 * the direction is a bitmask, and VERIFY_WRITE catches R/W
	 * transfers. `Type' is user-oriented, while
	 * access_ok is kernel-oriented, so the concept of "read" and
	 * "write" is reversed
	 */
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok_wrapper(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err =  !access_ok_wrapper(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (err) return -EFAULT;

	switch(cmd) {
		case SWAPHINTS_SEND_REQUEST:
			copy_from_user(&requests, (char *)arg, sizeof(struct swaphints_request));
			break;
		case SWAPHINTS_GET_RESPONSE:
			copy_to_user((char *)arg, &responses, sizeof(swaphints_request_t));
			break;
	  	default:  /* redundant, as cmd was checked against MAXNR */
			return -ENOTTY;
	}
	return retval;
}

struct file_operations swaphints_fops = {
	.owner =    THIS_MODULE,
	.unlocked_ioctl = swaphints_ioctl,
	.open =     swaphints_open,
	.release =  swaphints_release,
};

/*
 * Finally, the module stuff
 */

/*
 * The cleanup function is used to handle initialization failures as well.
 * Thefore, it must be careful to work correctly even if some of the items
 * have not been initialized
 */
void swaphints_cleanup_module(void)
{
	int i;
	dev_t devno = MKDEV(swaphints_major, swaphints_minor);

	/* Get rid of our char dev entries */
	if (swaphints_devices) {
		for (i = 0; i < swaphints_nr_devs; i++) {
			cdev_del(&swaphints_devices[i].cdev);
		}
		kfree(swaphints_devices);
	}

	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(devno, swaphints_nr_devs);

}


/*
 * Set up the char_dev structure for this device.
 */
static void swaphints_setup_cdev(struct swaphints_dev *dev, int index)
{
	int err, devno = MKDEV(swaphints_major, swaphints_minor + index);
    
	cdev_init(&dev->cdev, &swaphints_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add (&dev->cdev, devno, 1);
	/* Fail gracefully if need be */
	if (err)
		printk(KERN_NOTICE "Error %d adding swaphints%d", err, index);
}


int swaphints_init_module(void)
{
	int result, i;
	dev_t dev = 0;

	/*
	 * Get a range of minor numbers to work with, asking for a dynamic
	 * major unless directed otherwise at load time.
	 */
	if (swaphints_major) {
		dev = MKDEV(swaphints_major, swaphints_minor);
		result = register_chrdev_region(dev, swaphints_nr_devs, "swaphints");
	} else {
		result = alloc_chrdev_region(&dev, swaphints_minor, swaphints_nr_devs,
				"swaphints");
		swaphints_major = MAJOR(dev);
	}
	if (result < 0) {
		printk(KERN_WARNING "swaphints: can't get major %d\n", swaphints_major);
		return result;
	}

	/* 
	 * allocate the devices -- we can't have them static, as the number
	 * can be specified at load time
	 */
	swaphints_devices = kmalloc(swaphints_nr_devs * sizeof(struct swaphints_dev), GFP_KERNEL);
	if (!swaphints_devices) {
		result = -ENOMEM;
		goto fail;  /* Make this more graceful */
	}
	memset(swaphints_devices, 0, swaphints_nr_devs * sizeof(struct swaphints_dev));

        /* Initialize each device. */
	for (i = 0; i < swaphints_nr_devs; i++) {
		swaphints_setup_cdev(&swaphints_devices[i], i);
	}

        /* At this point call the init function for any friend device */
	dev = MKDEV(swaphints_major, swaphints_minor + swaphints_nr_devs);

	return 0; /* succeed */

  fail:
	swaphints_cleanup_module();
	return result;
}

module_init(swaphints_init_module);
module_exit(swaphints_cleanup_module);
