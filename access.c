/*
 * access.c -- the files with access control on open
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
 * $Id: access.c,v 1.17 2004/09/26 07:29:56 gregkh Exp $
 */

/* FIXME: cloned devices as a use for kobjects? */
 
#include <linux/kernel.h> /* printk() */
#include <linux/module.h>
#include <linux/slab.h>   /* kmalloc() */
#include <linux/fs.h>     /* everything... */
#include <linux/errno.h>  /* error codes */
#include <linux/types.h>  /* size_t */
#include <linux/fcntl.h>
#include <linux/cdev.h>
#include <linux/tty.h>
#include <asm/atomic.h>
#include <linux/list.h>
#include <linux/cred.h> /* current_uid(), current_euid() */
#include <linux/sched.h>
#include <linux/sched/signal.h>

#include "swaphints.h"        /* local definitions */

static dev_t swaphints_a_firstdev;  /* Where our range begins */

/*
 * These devices fall back on the main swaphints operations. They only
 * differ in the implementation of open() and close()
 */



/************************************************************************
 *
 * The first device is the single-open one,
 *  it has an hw structure and an open count
 */

static struct swaphints_dev swaphints_s_device;
static atomic_t swaphints_s_available = ATOMIC_INIT(1);

static int swaphints_s_open(struct inode *inode, struct file *filp)
{
	struct swaphints_dev *dev = &swaphints_s_device; /* device information */

	if (! atomic_dec_and_test (&swaphints_s_available)) {
		atomic_inc(&swaphints_s_available);
		return -EBUSY; /* already open */
	}

	/* then, everything else is copied from the bare swaphints device */
	// if ( (filp->f_flags & O_ACCMODE) == O_WRONLY)
	// 	swaphints_trim(dev);
	filp->private_data = dev;
	return 0;          /* success */
}

static int swaphints_s_release(struct inode *inode, struct file *filp)
{
	atomic_inc(&swaphints_s_available); /* release the device */
	return 0;
}


/*
 * The other operations for the single-open device come from the bare device
 */
struct file_operations swaphints_sngl_fops = {
	.owner =	THIS_MODULE,
	//.llseek =     	swaphints_llseek,
	//.read =       	swaphints_read,
	//.write =      	swaphints_write,
	.unlocked_ioctl = swaphints_ioctl,
	.open =       	swaphints_s_open,
	.release =    	swaphints_s_release,
};


/************************************************************************
 *
 * Next, the "uid" device. It can be opened multiple times by the
 * same user, but access is denied to other users if the device is open
 */

static struct swaphints_dev swaphints_u_device;
static int swaphints_u_count;	/* initialized to 0 by default */
static uid_t swaphints_u_owner;	/* initialized to 0 by default */
static DEFINE_SPINLOCK(swaphints_u_lock);

static int swaphints_u_open(struct inode *inode, struct file *filp)
{
	struct swaphints_dev *dev = &swaphints_u_device; /* device information */

	spin_lock(&swaphints_u_lock);
	if (swaphints_u_count && 
	                (swaphints_u_owner != current_uid().val) &&  /* allow user */
	                (swaphints_u_owner != current_euid().val) && /* allow whoever did su */
			!capable(CAP_DAC_OVERRIDE)) { /* still allow root */
		spin_unlock(&swaphints_u_lock);
		return -EBUSY;   /* -EPERM would confuse the user */
	}

	if (swaphints_u_count == 0)
		swaphints_u_owner = current_uid().val; /* grab it */

	swaphints_u_count++;
	spin_unlock(&swaphints_u_lock);

/* then, everything else is copied from the bare swaphints device */

	// if ((filp->f_flags & O_ACCMODE) == O_WRONLY)
	// 	swaphints_trim(dev);
	filp->private_data = dev;
	return 0;          /* success */
}

static int swaphints_u_release(struct inode *inode, struct file *filp)
{
	spin_lock(&swaphints_u_lock);
	swaphints_u_count--; /* nothing else */
	spin_unlock(&swaphints_u_lock);
	return 0;
}



/*
 * The other operations for the device come from the bare device
 */
struct file_operations swaphints_user_fops = {
	.owner =      THIS_MODULE,
	// .llseek =     swaphints_llseek,
	// .read =       swaphints_read,
	// .write =      swaphints_write,
	.unlocked_ioctl = swaphints_ioctl,
	.open =       swaphints_u_open,
	.release =    swaphints_u_release,
};


/************************************************************************
 *
 * Next, the device with blocking-open based on uid
 */

static struct swaphints_dev swaphints_w_device;
static int swaphints_w_count;	/* initialized to 0 by default */
static uid_t swaphints_w_owner;	/* initialized to 0 by default */
static DECLARE_WAIT_QUEUE_HEAD(swaphints_w_wait);
static DEFINE_SPINLOCK(swaphints_w_lock);

static inline int swaphints_w_available(void)
{
	return swaphints_w_count == 0 ||
		swaphints_w_owner == current_uid().val ||
		swaphints_w_owner == current_euid().val ||
		capable(CAP_DAC_OVERRIDE);
}


static int swaphints_w_open(struct inode *inode, struct file *filp)
{
	struct swaphints_dev *dev = &swaphints_w_device; /* device information */

	spin_lock(&swaphints_w_lock);
	while (! swaphints_w_available()) {
		spin_unlock(&swaphints_w_lock);
		if (filp->f_flags & O_NONBLOCK) return -EAGAIN;
		if (wait_event_interruptible (swaphints_w_wait, swaphints_w_available()))
			return -ERESTARTSYS; /* tell the fs layer to handle it */
		spin_lock(&swaphints_w_lock);
	}
	if (swaphints_w_count == 0)
		swaphints_w_owner = current_uid().val; /* grab it */
	swaphints_w_count++;
	spin_unlock(&swaphints_w_lock);

	/* then, everything else is copied from the bare swaphints device */
	// if ((filp->f_flags & O_ACCMODE) == O_WRONLY)
	// 	swaphints_trim(dev);
	filp->private_data = dev;
	return 0;          /* success */
}

static int swaphints_w_release(struct inode *inode, struct file *filp)
{
	int temp;

	spin_lock(&swaphints_w_lock);
	swaphints_w_count--;
	temp = swaphints_w_count;
	spin_unlock(&swaphints_w_lock);

	if (temp == 0)
		wake_up_interruptible_sync(&swaphints_w_wait); /* awake other uid's */
	return 0;
}


/*
 * The other operations for the device come from the bare device
 */
struct file_operations swaphints_wusr_fops = {
	.owner =      THIS_MODULE,
	// .llseek =     swaphints_llseek,
	// .read =       swaphints_read,
	// .write =      swaphints_write,
	.unlocked_ioctl = swaphints_ioctl,
	.open =       swaphints_w_open,
	.release =    swaphints_w_release,
};

/************************************************************************
 *
 * Finally the `cloned' private device. This is trickier because it
 * involves list management, and dynamic allocation.
 */

/* The clone-specific data structure includes a key field */

struct swaphints_listitem {
	struct swaphints_dev device;
	dev_t key;
	struct list_head list;
    
};

/* The list of devices, and a lock to protect it */
static LIST_HEAD(swaphints_c_list);
static DEFINE_SPINLOCK(swaphints_c_lock);

/* A placeholder swaphints_dev which really just holds the cdev stuff. */
static struct swaphints_dev swaphints_c_device;   

/* Look for a device or create one if missing */
static struct swaphints_dev *swaphints_c_lookfor_device(dev_t key)
{
	struct swaphints_listitem *lptr;

	list_for_each_entry(lptr, &swaphints_c_list, list) {
		if (lptr->key == key)
			return &(lptr->device);
	}

	/* not found */
	lptr = kmalloc(sizeof(struct swaphints_listitem), GFP_KERNEL);
	if (!lptr)
		return NULL;

	/* initialize the device */
	memset(lptr, 0, sizeof(struct swaphints_listitem));
	lptr->key = key;
	//swaphints_trim(&(lptr->device)); /* initialize it */
	mutex_init(&lptr->device.lock);

	/* place it in the list */
	list_add(&lptr->list, &swaphints_c_list);

	return &(lptr->device);
}

static int swaphints_c_open(struct inode *inode, struct file *filp)
{
	struct swaphints_dev *dev;
	dev_t key;
 
	if (!current->signal->tty) { 
		//PDEBUG("Process \"%s\" has no ctl tty\n", current->comm);
		return -EINVAL;
	}
	key = tty_devnum(current->signal->tty);

	/* look for a swaphintsc device in the list */
	spin_lock(&swaphints_c_lock);
	dev = swaphints_c_lookfor_device(key);
	spin_unlock(&swaphints_c_lock);

	if (!dev)
		return -ENOMEM;

	/* then, everything else is copied from the bare swaphints device */
	// if ( (filp->f_flags & O_ACCMODE) == O_WRONLY)
	// 	swaphints_trim(dev);
	filp->private_data = dev;
	return 0;          /* success */
}

static int swaphints_c_release(struct inode *inode, struct file *filp)
{
	/*
	 * Nothing to do, because the device is persistent.
	 * A `real' cloned device should be freed on last close
	 */
	return 0;
}



/*
 * The other operations for the device come from the bare device
 */
struct file_operations swaphints_priv_fops = {
	.owner =    THIS_MODULE,
	// .llseek =   swaphints_llseek,
	// .read =     swaphints_read,
	// .write =    swaphints_write,
	.unlocked_ioctl = swaphints_ioctl,
	.open =     swaphints_c_open,
	.release =  swaphints_c_release,
};

/************************************************************************
 *
 * And the init and cleanup functions come last
 */

static struct swaphints_adev_info {
	char *name;
	struct swaphints_dev *swaphintsdev;
	struct file_operations *fops;
} swaphints_access_devs[] = {
	{ "swaphintssingle", &swaphints_s_device, &swaphints_sngl_fops },
	{ "swaphintsuid", &swaphints_u_device, &swaphints_user_fops },
	{ "swaphintswuid", &swaphints_w_device, &swaphints_wusr_fops },
	{ "swaphintspriv", &swaphints_c_device, &swaphints_priv_fops }
};
#define SWAPHINTS_N_ADEVS 4

/*
 * Set up a single device.
 */
static void swaphints_access_setup (dev_t devno, struct swaphints_adev_info *devinfo)
{
	struct swaphints_dev *dev = devinfo->swaphintsdev;
	int err;

	/* Initialize the device structure */
	dev->quantum = swaphints_quantum;
	dev->qset = swaphints_qset;
	mutex_init(&dev->lock);

	/* Do the cdev stuff. */
	cdev_init(&dev->cdev, devinfo->fops);
	kobject_set_name(&dev->cdev.kobj, devinfo->name);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add (&dev->cdev, devno, 1);
        /* Fail gracefully if need be */
	if (err) {
		printk(KERN_NOTICE "Error %d adding %s\n", err, devinfo->name);
		kobject_put(&dev->cdev.kobj);
	} else
		printk(KERN_NOTICE "%s registered at %x\n", devinfo->name, devno);
}


int swaphints_access_init(dev_t firstdev)
{
	int result, i;

	/* Get our number space */
	result = register_chrdev_region (firstdev, SWAPHINTS_N_ADEVS, "swaphintsa");
	if (result < 0) {
		printk(KERN_WARNING "swaphintsa: device number registration failed\n");
		return 0;
	}
	swaphints_a_firstdev = firstdev;

	/* Set up each device. */
	for (i = 0; i < SWAPHINTS_N_ADEVS; i++)
		swaphints_access_setup (firstdev + i, swaphints_access_devs + i);
	return SWAPHINTS_N_ADEVS;
}

/*
 * This is called by cleanup_module or on failure.
 * It is required to never fail, even if nothing was initialized first
 */
void swaphints_access_cleanup(void)
{
	struct swaphints_listitem *lptr, *next;
	int i;

	/* Clean up the static devs */
	for (i = 0; i < SWAPHINTS_N_ADEVS; i++) {
		struct swaphints_dev *dev = swaphints_access_devs[i].swaphintsdev;
		cdev_del(&dev->cdev);
		//swaphints_trim(swaphints_access_devs[i].swaphintsdev);
	}

    	/* And all the cloned devices */
	list_for_each_entry_safe(lptr, next, &swaphints_c_list, list) {
		list_del(&lptr->list);
		//swaphints_trim(&(lptr->device));
		kfree(lptr);
	}

	/* Free up our number space */
	unregister_chrdev_region(swaphints_a_firstdev, SWAPHINTS_N_ADEVS);
	return;
}
