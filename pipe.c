/*
 * pipe.c -- fifo driver for swaphints
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

#include <linux/kernel.h>	/* printk(), min() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/proc_fs.h>
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/seq_file.h>

#include "proc_ops_version.h"

#include "swaphints.h"		/* local definitions */

struct swaphints_pipe {
        wait_queue_head_t inq, outq;       /* read and write queues */
        char *buffer, *end;                /* begin of buf, end of buf */
        int buffersize;                    /* used in pointer arithmetic */
        char *rp, *wp;                     /* where to read, where to write */
        int nreaders, nwriters;            /* number of openings for r/w */
        struct fasync_struct *async_queue; /* asynchronous readers */
        struct mutex lock;              /* mutual exclusion mutex */
        struct cdev cdev;                  /* Char device structure */
};

/* parameters */
static int swaphints_p_nr_devs = SWAPHINTS_P_NR_DEVS;	/* number of pipe devices */
int swaphints_p_buffer =  SWAPHINTS_P_BUFFER;	/* buffer size */
dev_t swaphints_p_devno;			/* Our first device number */

module_param(swaphints_p_nr_devs, int, 0);	/* FIXME check perms */
module_param(swaphints_p_buffer, int, 0);

static struct swaphints_pipe *swaphints_p_devices;

static int swaphints_p_fasync(int fd, struct file *filp, int mode);
static int spacefree(struct swaphints_pipe *dev);
/*
 * Open and close
 */


static int swaphints_p_open(struct inode *inode, struct file *filp)
{
	struct swaphints_pipe *dev;

	dev = container_of(inode->i_cdev, struct swaphints_pipe, cdev);
	filp->private_data = dev;

	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;
	if (!dev->buffer) {
		/* allocate the buffer */
		dev->buffer = kmalloc(swaphints_p_buffer, GFP_KERNEL);
		if (!dev->buffer) {
			mutex_unlock(&dev->lock);
			return -ENOMEM;
		}
	}
	dev->buffersize = swaphints_p_buffer;
	dev->end = dev->buffer + dev->buffersize;
	dev->rp = dev->wp = dev->buffer; /* rd and wr from the beginning */

	/* use f_mode,not  f_flags: it's cleaner (fs/open.c tells why) */
	if (filp->f_mode & FMODE_READ)
		dev->nreaders++;
	if (filp->f_mode & FMODE_WRITE)
		dev->nwriters++;
	mutex_unlock(&dev->lock);

	return nonseekable_open(inode, filp);
}



static int swaphints_p_release(struct inode *inode, struct file *filp)
{
	struct swaphints_pipe *dev = filp->private_data;

	/* remove this filp from the asynchronously notified filp's */
	swaphints_p_fasync(-1, filp, 0);
	mutex_lock(&dev->lock);
	if (filp->f_mode & FMODE_READ)
		dev->nreaders--;
	if (filp->f_mode & FMODE_WRITE)
		dev->nwriters--;
	if (dev->nreaders + dev->nwriters == 0) {
		kfree(dev->buffer);
		dev->buffer = NULL; /* the other fields are not checked on open */
	}
	mutex_unlock(&dev->lock);
	return 0;
}


/*
 * Data management: read and write
 */

static ssize_t swaphints_p_read (struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
	struct swaphints_pipe *dev = filp->private_data;

	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;

	while (dev->rp == dev->wp) { /* nothing to read */
		mutex_unlock(&dev->lock); /* release the lock */
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		PDEBUG("\"%s\" reading: going to sleep\n", current->comm);
		if (wait_event_interruptible(dev->inq, (dev->rp != dev->wp)))
			return -ERESTARTSYS; /* signal: tell the fs layer to handle it */
		/* otherwise loop, but first reacquire the lock */
		if (mutex_lock_interruptible(&dev->lock))
			return -ERESTARTSYS;
	}
	/* ok, data is there, return something */
	if (dev->wp > dev->rp)
		count = min(count, (size_t)(dev->wp - dev->rp));
	else /* the write pointer has wrapped, return data up to dev->end */
		count = min(count, (size_t)(dev->end - dev->rp));
	if (copy_to_user(buf, dev->rp, count)) {
		mutex_unlock (&dev->lock);
		return -EFAULT;
	}
	dev->rp += count;
	if (dev->rp == dev->end)
		dev->rp = dev->buffer; /* wrapped */
	mutex_unlock (&dev->lock);

	/* finally, awake any writers and return */
	wake_up_interruptible(&dev->outq);
	PDEBUG("\"%s\" did read %li bytes\n",current->comm, (long)count);
	return count;
}

/* Wait for space for writing; caller must hold device semaphore.  On
 * error the semaphore will be released before returning. */
static int swaphints_getwritespace(struct swaphints_pipe *dev, struct file *filp)
{
	while (spacefree(dev) == 0) { /* full */
		DEFINE_WAIT(wait);
		
		mutex_unlock(&dev->lock);
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		PDEBUG("\"%s\" writing: going to sleep\n",current->comm);
		prepare_to_wait(&dev->outq, &wait, TASK_INTERRUPTIBLE);
		if (spacefree(dev) == 0)
			schedule();
		finish_wait(&dev->outq, &wait);
		if (signal_pending(current))
			return -ERESTARTSYS; /* signal: tell the fs layer to handle it */
		if (mutex_lock_interruptible(&dev->lock))
			return -ERESTARTSYS;
	}
	return 0;
}	

/* How much space is free? */
static int spacefree(struct swaphints_pipe *dev)
{
	if (dev->rp == dev->wp)
		return dev->buffersize - 1;
	return ((dev->rp + dev->buffersize - dev->wp) % dev->buffersize) - 1;
}

static ssize_t swaphints_p_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
	struct swaphints_pipe *dev = filp->private_data;
	int result;

	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;

	/* Make sure there's space to write */
	result = swaphints_getwritespace(dev, filp);
	if (result)
		return result; /* swaphints_getwritespace called up(&dev->sem) */

	/* ok, space is there, accept something */
	count = min(count, (size_t)spacefree(dev));
	if (dev->wp >= dev->rp)
		count = min(count, (size_t)(dev->end - dev->wp)); /* to end-of-buf */
	else /* the write pointer has wrapped, fill up to rp-1 */
		count = min(count, (size_t)(dev->rp - dev->wp - 1));
	PDEBUG("Going to accept %li bytes to %p from %p\n", (long)count, dev->wp, buf);
	if (copy_from_user(dev->wp, buf, count)) {
		mutex_unlock(&dev->lock);
		return -EFAULT;
	}
	dev->wp += count;
	if (dev->wp == dev->end)
		dev->wp = dev->buffer; /* wrapped */
	mutex_unlock(&dev->lock);

	/* finally, awake any reader */
	wake_up_interruptible(&dev->inq);  /* blocked in read() and select() */

	/* and signal asynchronous readers, explained late in chapter 5 */
	if (dev->async_queue)
		kill_fasync(&dev->async_queue, SIGIO, POLL_IN);
	PDEBUG("\"%s\" did write %li bytes\n",current->comm, (long)count);
	return count;
}

static unsigned int swaphints_p_poll(struct file *filp, poll_table *wait)
{
	struct swaphints_pipe *dev = filp->private_data;
	unsigned int mask = 0;

	/*
	 * The buffer is circular; it is considered full
	 * if "wp" is right behind "rp" and empty if the
	 * two are equal.
	 */
	mutex_lock(&dev->lock);
	poll_wait(filp, &dev->inq,  wait);
	poll_wait(filp, &dev->outq, wait);
	if (dev->rp != dev->wp)
		mask |= POLLIN | POLLRDNORM;	/* readable */
	if (spacefree(dev))
		mask |= POLLOUT | POLLWRNORM;	/* writable */
	mutex_unlock(&dev->lock);
	return mask;
}





static int swaphints_p_fasync(int fd, struct file *filp, int mode)
{
	struct swaphints_pipe *dev = filp->private_data;

	return fasync_helper(fd, filp, mode, &dev->async_queue);
}



/* FIXME this should use seq_file */
#ifdef SWAPHINTS_DEBUG

static int swaphints_read_p_mem(struct seq_file *s, void *v)
{
	int i;
	struct swaphints_pipe *p;

#define LIMIT (PAGE_SIZE-200)        /* don't print any more after this size */
	seq_printf(s, "Default buffersize is %i\n", swaphints_p_buffer);
	for(i = 0; i<swaphints_p_nr_devs && s->count <= LIMIT; i++) {
		p = &swaphints_p_devices[i];
		if (mutex_lock_interruptible(&p->lock))
			return -ERESTARTSYS;
		seq_printf(s, "\nDevice %i: %p\n", i, p);
/*		seq_printf(s, "   Queues: %p %p\n", p->inq, p->outq);*/
		seq_printf(s, "   Buffer: %p to %p (%i bytes)\n", p->buffer, p->end, p->buffersize);
		seq_printf(s, "   rp %p   wp %p\n", p->rp, p->wp);
		seq_printf(s, "   readers %i   writers %i\n", p->nreaders, p->nwriters);
		mutex_unlock(&p->lock);
	}
	return 0;
}

static int swaphintspipe_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, swaphints_read_p_mem, NULL);
}

static struct file_operations swaphintspipe_proc_ops = {
	.owner   = THIS_MODULE,
	.open    = swaphintspipe_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release
};

#endif



/*
 * The file operations for the pipe device
 * (some are overlayed with bare swaphints)
 */
struct file_operations swaphints_pipe_fops = {
	.owner =	THIS_MODULE,
	.llseek =	no_llseek,
	.read =		swaphints_p_read,
	.write =	swaphints_p_write,
	.poll =		swaphints_p_poll,
	.unlocked_ioctl = swaphints_ioctl,
	.open =		swaphints_p_open,
	.release =	swaphints_p_release,
	.fasync =	swaphints_p_fasync,
};


/*
 * Set up a cdev entry.
 */
static void swaphints_p_setup_cdev(struct swaphints_pipe *dev, int index)
{
	int err, devno = swaphints_p_devno + index;
    
	cdev_init(&dev->cdev, &swaphints_pipe_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add (&dev->cdev, devno, 1);
	/* Fail gracefully if need be */
	if (err)
		printk(KERN_NOTICE "Error %d adding swaphintspipe%d", err, index);
}

 

/*
 * Initialize the pipe devs; return how many we did.
 */
int swaphints_p_init(dev_t firstdev)
{
	int i, result;

	result = register_chrdev_region(firstdev, swaphints_p_nr_devs, "swaphintsp");
	if (result < 0) {
		printk(KERN_NOTICE "Unable to get swaphintsp region, error %d\n", result);
		return 0;
	}
	swaphints_p_devno = firstdev;
	swaphints_p_devices = kmalloc(swaphints_p_nr_devs * sizeof(struct swaphints_pipe), GFP_KERNEL);
	if (swaphints_p_devices == NULL) {
		unregister_chrdev_region(firstdev, swaphints_p_nr_devs);
		return 0;
	}
	memset(swaphints_p_devices, 0, swaphints_p_nr_devs * sizeof(struct swaphints_pipe));
	for (i = 0; i < swaphints_p_nr_devs; i++) {
		init_waitqueue_head(&(swaphints_p_devices[i].inq));
		init_waitqueue_head(&(swaphints_p_devices[i].outq));
		mutex_init(&swaphints_p_devices[i].lock);
		swaphints_p_setup_cdev(swaphints_p_devices + i, i);
	}
#ifdef SWAPHINTS_DEBUG
	proc_create("swaphintspipe", 0, NULL, proc_ops_wrapper(&swaphintspipe_proc_ops,swaphintspipe_pops));
#endif
	return swaphints_p_nr_devs;
}

/*
 * This is called by cleanup_module or on failure.
 * It is required to never fail, even if nothing was initialized first
 */
void swaphints_p_cleanup(void)
{
	int i;

#ifdef SWAPHINTS_DEBUG
	remove_proc_entry("swaphintspipe", NULL);
#endif

	if (!swaphints_p_devices)
		return; /* nothing else to release */

	for (i = 0; i < swaphints_p_nr_devs; i++) {
		cdev_del(&swaphints_p_devices[i].cdev);
		kfree(swaphints_p_devices[i].buffer);
	}
	kfree(swaphints_p_devices);
	unregister_chrdev_region(swaphints_p_devno, swaphints_p_nr_devs);
	swaphints_p_devices = NULL; /* pedantic */
}
