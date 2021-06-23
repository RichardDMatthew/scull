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
int swaphints_quantum = SWAPHINTS_QUANTUM;
int swaphints_qset =    SWAPHINTS_QSET;

module_param(swaphints_major, int, S_IRUGO);
module_param(swaphints_minor, int, S_IRUGO);
module_param(swaphints_nr_devs, int, S_IRUGO);
module_param(swaphints_quantum, int, S_IRUGO);
module_param(swaphints_qset, int, S_IRUGO);

MODULE_AUTHOR("Alessandro Rubini, Jonathan Corbet");
MODULE_LICENSE("Dual BSD/GPL");

struct swaphints_dev *swaphints_devices;	/* allocated in swaphints_init_module */


/*
 * Empty out the swaphints device; must be called with the device
 * semaphore held.
 */
// int swaphints_trim(struct swaphints_dev *dev)
// {
// 	struct swaphints_qset *next, *dptr;
// 	int qset = dev->qset;   /* "dev" is not-null */
// 	int i;

// 	for (dptr = dev->data; dptr; dptr = next) { /* all the list items */
// 		if (dptr->data) {
// 			for (i = 0; i < qset; i++)
// 				kfree(dptr->data[i]);
// 			kfree(dptr->data);
// 			dptr->data = NULL;
// 		}
// 		next = dptr->next;
// 		kfree(dptr);
// 	}
// 	dev->size = 0;
// 	dev->quantum = swaphints_quantum;
// 	dev->qset = swaphints_qset;
// 	dev->data = NULL;
// 	return 0;
//}
// #ifdef SWAPHINTS_DEBUG /* use proc only if debugging */
// /*
//  * The proc filesystem: function to read and entry
//  */

// int swaphints_read_procmem(struct seq_file *s, void *v)
// {
//         int i, j;
//         int limit = s->size - 80; /* Don't print more than this */

//         for (i = 0; i < swaphints_nr_devs && s->count <= limit; i++) {
//                 struct swaphints_dev *d = &swaphints_devices[i];
//                 struct swaphints_qset *qs = d->data;
//                 if (mutex_lock_interruptible(&d->lock))
//                         return -ERESTARTSYS;
//                 seq_printf(s,"\nDevice %i: qset %i, q %i, sz %li\n",
//                              i, d->qset, d->quantum, d->size);
//                 for (; qs && s->count <= limit; qs = qs->next) { /* scan the list */
//                         seq_printf(s, "  item at %p, qset at %p\n",
//                                      qs, qs->data);
//                         if (qs->data && !qs->next) /* dump only the last item */
//                                 for (j = 0; j < d->qset; j++) {
//                                         if (qs->data[j])
//                                                 seq_printf(s, "    % 4i: %8p\n",
//                                                              j, qs->data[j]);
//                                 }
//                 }
//                 mutex_unlock(&swaphints_devices[i].lock);
//         }
//         return 0;
// }



// /*
//  * Here are our sequence iteration methods.  Our "position" is
//  * simply the device number.
//  */
// static void *swaphints_seq_start(struct seq_file *s, loff_t *pos)
// {
// 	if (*pos >= swaphints_nr_devs)
// 		return NULL;   /* No more to read */
// 	return swaphints_devices + *pos;
// }

// static void *swaphints_seq_next(struct seq_file *s, void *v, loff_t *pos)
// {
// 	(*pos)++;
// 	if (*pos >= swaphints_nr_devs)
// 		return NULL;
// 	return swaphints_devices + *pos;
// }

// static void swaphints_seq_stop(struct seq_file *s, void *v)
// {
// 	/* Actually, there's nothing to do here */
// }

// static int swaphints_seq_show(struct seq_file *s, void *v)
// {
// 	struct swaphints_dev *dev = (struct swaphints_dev *) v;
// 	struct swaphints_qset *d;
// 	int i;

// 	if (mutex_lock_interruptible(&dev->lock))
// 		return -ERESTARTSYS;
// 	seq_printf(s, "\nDevice %i: qset %i, q %i, sz %li\n",
// 			(int) (dev - swaphints_devices), dev->qset,
// 			dev->quantum, dev->size);
// 	for (d = dev->data; d; d = d->next) { /* scan the list */
// 		seq_printf(s, "  item at %p, qset at %p\n", d, d->data);
// 		if (d->data && !d->next) /* dump only the last item */
// 			for (i = 0; i < dev->qset; i++) {
// 				if (d->data[i])
// 					seq_printf(s, "    % 4i: %8p\n",
// 							i, d->data[i]);
// 			}
// 	}
// 	mutex_unlock(&dev->lock);
// 	return 0;
// }
	
// /*
//  * Tie the sequence operators up.
//  */
// static struct seq_operations swaphints_seq_ops = {
// 	.start = swaphints_seq_start,
// 	.next  = swaphints_seq_next,
// 	.stop  = swaphints_seq_stop,
// 	.show  = swaphints_seq_show
// };

// /*
//  * Now to implement the /proc files we need only make an open
//  * method which sets up the sequence operators.
//  */
// static int swaphintsmem_proc_open(struct inode *inode, struct file *file)
// {
// 	return single_open(file, swaphints_read_procmem, NULL);
// }

// static int swaphintsseq_proc_open(struct inode *inode, struct file *file)
// {
// 	return seq_open(file, &swaphints_seq_ops);
// }

// /*
//  * Create a set of file operations for our proc files.
//  */
// static struct file_operations swaphintsmem_proc_ops = {
// 	.owner   = THIS_MODULE,
// 	.open    = swaphintsmem_proc_open,
// 	.read    = seq_read,
// 	.llseek  = seq_lseek,
// 	.release = single_release
// };

// static struct file_operations swaphintsseq_proc_ops = {
// 	.owner   = THIS_MODULE,
// 	.open    = swaphintsseq_proc_open,
// 	.read    = seq_read,
// 	.llseek  = seq_lseek,
// 	.release = seq_release
// };
	

// /*
//  * Actually create (and remove) the /proc file(s).
//  */

// static void swaphints_create_proc(void)
// {
// 	proc_create_data("swaphintsmem", 0 /* default mode */,
// 			NULL /* parent dir */, proc_ops_wrapper(&swaphintsmem_proc_ops, swaphintsmem_pops),
// 			NULL /* client data */);
// 	proc_create("swaphintsseq", 0, NULL, proc_ops_wrapper(&swaphintsseq_proc_ops, swaphintsseq_pops));
// }

// static void swaphints_remove_proc(void)
// {
// 	/* no problem if it was not registered */
// 	remove_proc_entry("swaphintsmem", NULL /* parent dir */);
// 	remove_proc_entry("swaphintsseq", NULL);
// }


// #endif /* SWAPHINTS_DEBUG */





/*
 * Open and close
 */

int swaphints_open(struct inode *inode, struct file *filp)
{
	struct swaphints_dev *dev; /* device information */

	dev = container_of(inode->i_cdev, struct swaphints_dev, cdev);
	filp->private_data = dev; /* for other methods */

	/* now trim to 0 the length of the device if open was write-only */
	if ( (filp->f_flags & O_ACCMODE) == O_WRONLY) {
		if (mutex_lock_interruptible(&dev->lock))
			return -ERESTARTSYS;
		//swaphints_trim(dev); /* ignore errors */
		mutex_unlock(&dev->lock);
	}
	return 0;          /* success */
}

int swaphints_release(struct inode *inode, struct file *filp)
{
	return 0;
}
/*
 * Follow the list
 */
// struct swaphints_qset *swaphints_follow(struct swaphints_dev *dev, int n)
// {
// 	struct swaphints_qset *qs = dev->data;

//         /* Allocate first qset explicitly if need be */
// 	if (! qs) {
// 		qs = dev->data = kmalloc(sizeof(struct swaphints_qset), GFP_KERNEL);
// 		if (qs == NULL)
// 			return NULL;  /* Never mind */
// 		memset(qs, 0, sizeof(struct swaphints_qset));
// 	}

// 	/* Then follow the list */
// 	while (n--) {
// 		if (!qs->next) {
// 			qs->next = kmalloc(sizeof(struct swaphints_qset), GFP_KERNEL);
// 			if (qs->next == NULL)
// 				return NULL;  /* Never mind */
// 			memset(qs->next, 0, sizeof(struct swaphints_qset));
// 		}
// 		qs = qs->next;
// 		continue;
// 	}
// 	return qs;
// }

/*
 * Data management: read and write
 */

// ssize_t swaphints_read(struct file *filp, char __user *buf, size_t count,
//                 loff_t *f_pos)
// {
// 	struct swaphints_dev *dev = filp->private_data; 
// 	struct swaphints_qset *dptr;	/* the first listitem */
// 	int quantum = dev->quantum, qset = dev->qset;
// 	int itemsize = quantum * qset; /* how many bytes in the listitem */
// 	int item, s_pos, q_pos, rest;
// 	ssize_t retval = 0;

// 	if (mutex_lock_interruptible(&dev->lock))
// 		return -ERESTARTSYS;
// 	if (*f_pos >= dev->size)
// 		goto out;
// 	if (*f_pos + count > dev->size)
// 		count = dev->size - *f_pos;

// 	/* find listitem, qset index, and offset in the quantum */
// 	item = (long)*f_pos / itemsize;
// 	rest = (long)*f_pos % itemsize;
// 	s_pos = rest / quantum; q_pos = rest % quantum;

// 	/* follow the list up to the right position (defined elsewhere) */
// 	dptr = swaphints_follow(dev, item);

// 	if (dptr == NULL || !dptr->data || ! dptr->data[s_pos])
// 		goto out; /* don't fill holes */

// 	/* read only up to the end of this quantum */
// 	if (count > quantum - q_pos)
// 		count = quantum - q_pos;

// 	if (copy_to_user(buf, dptr->data[s_pos] + q_pos, count)) {
// 		retval = -EFAULT;
// 		goto out;
// 	}
// 	*f_pos += count;
// 	retval = count;

//   out:
// 	mutex_unlock(&dev->lock);
// 	return retval;
// }

// ssize_t swaphints_write(struct file *filp, const char __user *buf, size_t count,
//                 loff_t *f_pos)
// {
// 	struct swaphints_dev *dev = filp->private_data;
// 	struct swaphints_qset *dptr;
// 	int quantum = dev->quantum, qset = dev->qset;
// 	int itemsize = quantum * qset;
// 	int item, s_pos, q_pos, rest;
// 	ssize_t retval = -ENOMEM; /* value used in "goto out" statements */

// 	if (mutex_lock_interruptible(&dev->lock))
// 		return -ERESTARTSYS;

// 	/* find listitem, qset index and offset in the quantum */
// 	item = (long)*f_pos / itemsize;
// 	rest = (long)*f_pos % itemsize;
// 	s_pos = rest / quantum; q_pos = rest % quantum;

// 	/* follow the list up to the right position */
// 	dptr = swaphints_follow(dev, item);
// 	if (dptr == NULL)
// 		goto out;
// 	if (!dptr->data) {
// 		dptr->data = kmalloc(qset * sizeof(char *), GFP_KERNEL);
// 		if (!dptr->data)
// 			goto out;
// 		memset(dptr->data, 0, qset * sizeof(char *));
// 	}
// 	if (!dptr->data[s_pos]) {
// 		dptr->data[s_pos] = kmalloc(quantum, GFP_KERNEL);
// 		if (!dptr->data[s_pos])
// 			goto out;
// 	}
// 	/* write only up to the end of this quantum */
// 	if (count > quantum - q_pos)
// 		count = quantum - q_pos;

// 	if (copy_from_user(dptr->data[s_pos]+q_pos, buf, count)) {
// 		retval = -EFAULT;
// 		goto out;
// 	}
// 	*f_pos += count;
// 	retval = count;

//         /* update the size */
// 	if (dev->size < *f_pos)
// 		dev->size = *f_pos;

//   out:
// 	mutex_unlock(&dev->lock);
// 	return retval;
// }

/*
 * The ioctl() implementation
 */

long swaphints_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

	int err = 0, tmp;
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

	  case SWAPHINTS_IOCRESET:
		swaphints_quantum = SWAPHINTS_QUANTUM;
		swaphints_qset = SWAPHINTS_QSET;
		break;
        
	  case SWAPHINTS_IOCSQUANTUM: /* Set: arg points to the value */
		if (! capable (CAP_SYS_ADMIN))
			return -EPERM;
		retval = __get_user(swaphints_quantum, (int __user *)arg);
		break;

	  case SWAPHINTS_IOCTQUANTUM: /* Tell: arg is the value */
		if (! capable (CAP_SYS_ADMIN))
			return -EPERM;
		swaphints_quantum = arg;
		break;

	  case SWAPHINTS_IOCGQUANTUM: /* Get: arg is pointer to result */
		retval = __put_user(swaphints_quantum, (int __user *)arg);
		break;

	  case SWAPHINTS_IOCQQUANTUM: /* Query: return it (it's positive) */
		return swaphints_quantum;

	  case SWAPHINTS_IOCXQUANTUM: /* eXchange: use arg as pointer */
		if (! capable (CAP_SYS_ADMIN))
			return -EPERM;
		tmp = swaphints_quantum;
		retval = __get_user(swaphints_quantum, (int __user *)arg);
		if (retval == 0)
			retval = __put_user(tmp, (int __user *)arg);
		break;

	  case SWAPHINTS_IOCHQUANTUM: /* sHift: like Tell + Query */
		if (! capable (CAP_SYS_ADMIN))
			return -EPERM;
		tmp = swaphints_quantum;
		swaphints_quantum = arg;
		return tmp;
        
	  case SWAPHINTS_IOCSQSET:
		if (! capable (CAP_SYS_ADMIN))
			return -EPERM;
		retval = __get_user(swaphints_qset, (int __user *)arg);
		break;

	  case SWAPHINTS_IOCTQSET:
		if (! capable (CAP_SYS_ADMIN))
			return -EPERM;
		swaphints_qset = arg;
		break;

	  case SWAPHINTS_IOCGQSET:
		retval = __put_user(swaphints_qset, (int __user *)arg);
		break;

	  case SWAPHINTS_IOCQQSET:
		return swaphints_qset;

	  case SWAPHINTS_IOCXQSET:
		if (! capable (CAP_SYS_ADMIN))
			return -EPERM;
		tmp = swaphints_qset;
		retval = __get_user(swaphints_qset, (int __user *)arg);
		if (retval == 0)
			retval = put_user(tmp, (int __user *)arg);
		break;

	  case SWAPHINTS_IOCHQSET:
		if (! capable (CAP_SYS_ADMIN))
			return -EPERM;
		tmp = swaphints_qset;
		swaphints_qset = arg;
		return tmp;

        /*
         * The following two change the buffer size for swaphintspipe.
         * The swaphintspipe device uses this same ioctl method, just to
         * write less code. Actually, it's the same driver, isn't it?
         */

	  case SWAPHINTS_P_IOCTSIZE:
		swaphints_p_buffer = arg;
		break;

	  case SWAPHINTS_P_IOCQSIZE:
		return swaphints_p_buffer;


	  default:  /* redundant, as cmd was checked against MAXNR */
		return -ENOTTY;
	}
	return retval;

}



/*
 * The "extended" operations -- only seek
 */

// loff_t swaphints_llseek(struct file *filp, loff_t off, int whence)
// {
// 	struct swaphints_dev *dev = filp->private_data;
// 	loff_t newpos;

// 	switch(whence) {
// 	  case 0: /* SEEK_SET */
// 		newpos = off;
// 		break;

// 	  case 1: /* SEEK_CUR */
// 		newpos = filp->f_pos + off;
// 		break;

// 	  case 2: /* SEEK_END */
// 		newpos = dev->size + off;
// 		break;

// 	  default: /* can't happen */
// 		return -EINVAL;
// 	}
// 	if (newpos < 0) return -EINVAL;
// 	filp->f_pos = newpos;
// 	return newpos;
// }



struct file_operations swaphints_fops = {
	.owner =    THIS_MODULE,
	//.llseek =   swaphints_llseek,
	//.read =     swaphints_read,
	//.write =    swaphints_write,
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
			//swaphints_trim(swaphints_devices + i);
			cdev_del(&swaphints_devices[i].cdev);
		}
		kfree(swaphints_devices);
	}

// #ifdef SWAPHINTS_DEBUG /* use proc only if debugging */
// 	swaphints_remove_proc();
// #endif

	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(devno, swaphints_nr_devs);

	/* and call the cleanup functions for friend devices */
	swaphints_p_cleanup();
	swaphints_access_cleanup();

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
		result = register_chrdev_region(dev, swaphints_nr_devs, "scull");
	} else {
		result = alloc_chrdev_region(&dev, swaphints_minor, swaphints_nr_devs,
				"scull");
		swaphints_major = MAJOR(dev);
	}
	if (result < 0) {
		printk(KERN_WARNING "scull: can't get major %d\n", swaphints_major);
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
		swaphints_devices[i].quantum = swaphints_quantum;
		swaphints_devices[i].qset = swaphints_qset;
		mutex_init(&swaphints_devices[i].lock);
		swaphints_setup_cdev(&swaphints_devices[i], i);
	}

        /* At this point call the init function for any friend device */
	dev = MKDEV(swaphints_major, swaphints_minor + swaphints_nr_devs);
	dev += swaphints_p_init(dev);
	dev += swaphints_access_init(dev);

// #ifdef SWAPHINTS_DEBUG /* only when debugging */
// 	swaphints_create_proc();
// #endif

	return 0; /* succeed */

  fail:
	swaphints_cleanup_module();
	return result;
}

module_init(swaphints_init_module);
module_exit(swaphints_cleanup_module);
