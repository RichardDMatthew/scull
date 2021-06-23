/*
 * swaphints.h -- definitions for the char module
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
 * $Id: swaphints.h,v 1.15 2004/11/04 17:51:18 rubini Exp $
 */

#ifndef _SWAPHINTS_H_
#define _SWAPHINTS_H_

#include <linux/ioctl.h> /* needed for the _IOW etc stuff used later */

/*
 * Macros to help debugging
 */

//#undef PDEBUG             /* undef it, just in case */
// #ifdef SWAPHINTS_DEBUG
// #  ifdef __KERNEL__
//      /* This one if debugging is on, and kernel space */
// #    define PDEBUG(fmt, args...) printk( KERN_DEBUG "swaphints: " fmt, ## args)
// #  else
//      /* This one for user space */
// #    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
// #  endif
// #else
// #  define PDEBUG(fmt, args...) /* not debugging: nothing */
// #endif

// #undef PDEBUGG
// #define PDEBUGG(fmt, args...) /* nothing: it's a placeholder */

#ifndef SWAPHINTS_MAJOR
#define SWAPHINTS_MAJOR 0   /* dynamic major by default */
#endif

#ifndef SWAPHINTS_NR_DEVS
#define SWAPHINTS_NR_DEVS 4    /* swaphints0 through swaphints3 */
#endif

#ifndef SWAPHINTS_P_NR_DEVS
#define SWAPHINTS_P_NR_DEVS 4  /* swaphintspipe0 through swaphintspipe3 */
#endif

/*
 * The bare device is a variable-length region of memory.
 * Use a linked list of indirect blocks.
 *
 * "swaphints_dev->data" points to an array of pointers, each
 * pointer refers to a memory area of SWAPHINTS_QUANTUM bytes.
 *
 * The array (quantum-set) is SWAPHINTS_QSET long.
 */
#ifndef SWAPHINTS_QUANTUM
#define SWAPHINTS_QUANTUM 4000
#endif

#ifndef SWAPHINTS_QSET
#define SWAPHINTS_QSET    1000
#endif

/*
 * The pipe device is a simple circular buffer. Here its default size
 */
#ifndef SWAPHINTS_P_BUFFER
#define SWAPHINTS_P_BUFFER 4000
#endif

/*
 * Representation of swaphints quantum sets.
 */
struct swaphints_qset {
	void **data;
	struct swaphints_qset *next;
};

struct swaphints_dev {
	struct swaphints_qset *data;  /* Pointer to first quantum set */
	int quantum;              /* the current quantum size */
	int qset;                 /* the current array size */
	unsigned long size;       /* amount of data stored here */
	unsigned int access_key;  /* used by swaphintsuid and swaphintspriv */
	struct mutex lock;     /* mutual exclusion semaphore     */
	struct cdev cdev;	  /* Char device structure		*/
};

/*
 * Split minors in two parts
 */
#define TYPE(minor)	(((minor) >> 4) & 0xf)	/* high nibble */
#define NUM(minor)	((minor) & 0xf)		/* low  nibble */


/*
 * The different configurable parameters
 */
extern int swaphints_major;     /* main.c */
extern int swaphints_nr_devs;
extern int swaphints_quantum;
extern int swaphints_qset;

extern int swaphints_p_buffer;	/* pipe.c */


/*
 * Prototypes for shared functions
 */

int     swaphints_p_init(dev_t dev);
void    swaphints_p_cleanup(void);
int     swaphints_access_init(dev_t dev);
void    swaphints_access_cleanup(void);

// int     swaphints_trim(struct swaphints_dev *dev);

// ssize_t swaphints_read(struct file *filp, char __user *buf, size_t count,
//                    loff_t *f_pos);
// ssize_t swaphints_write(struct file *filp, const char __user *buf, size_t count,
//                     loff_t *f_pos);
// loff_t  swaphints_llseek(struct file *filp, loff_t off, int whence);
long     swaphints_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);


/*
 * Ioctl definitions
 */

/* Use 'k' as magic number */
#define SWAPHINTS_IOC_MAGIC  'k'
/* Please use a different 8-bit number in your code */

#define SWAPHINTS_IOCRESET    _IO(SWAPHINTS_IOC_MAGIC, 0)

/*
 * S means "Set" through a ptr,
 * T means "Tell" directly with the argument value
 * G means "Get": reply by setting through a pointer
 * Q means "Query": response is on the return value
 * X means "eXchange": switch G and S atomically
 * H means "sHift": switch T and Q atomically
 */
#define SWAPHINTS_IOCSQUANTUM _IOW(SWAPHINTS_IOC_MAGIC,  1, int)
#define SWAPHINTS_IOCSQSET    _IOW(SWAPHINTS_IOC_MAGIC,  2, int)
#define SWAPHINTS_IOCTQUANTUM _IO(SWAPHINTS_IOC_MAGIC,   3)
#define SWAPHINTS_IOCTQSET    _IO(SWAPHINTS_IOC_MAGIC,   4)
#define SWAPHINTS_IOCGQUANTUM _IOR(SWAPHINTS_IOC_MAGIC,  5, int)
#define SWAPHINTS_IOCGQSET    _IOR(SWAPHINTS_IOC_MAGIC,  6, int)
#define SWAPHINTS_IOCQQUANTUM _IO(SWAPHINTS_IOC_MAGIC,   7)
#define SWAPHINTS_IOCQQSET    _IO(SWAPHINTS_IOC_MAGIC,   8)
#define SWAPHINTS_IOCXQUANTUM _IOWR(SWAPHINTS_IOC_MAGIC, 9, int)
#define SWAPHINTS_IOCXQSET    _IOWR(SWAPHINTS_IOC_MAGIC,10, int)
#define SWAPHINTS_IOCHQUANTUM _IO(SWAPHINTS_IOC_MAGIC,  11)
#define SWAPHINTS_IOCHQSET    _IO(SWAPHINTS_IOC_MAGIC,  12)

/*
 * The other entities only have "Tell" and "Query", because they're
 * not printed in the book, and there's no need to have all six.
 * (The previous stuff was only there to show different ways to do it.
 */
#define SWAPHINTS_P_IOCTSIZE _IO(SWAPHINTS_IOC_MAGIC,   13)
#define SWAPHINTS_P_IOCQSIZE _IO(SWAPHINTS_IOC_MAGIC,   14)
/* ... more to come */

#define SWAPHINTS_IOC_MAXNR 14


/* start of swaphints structs */
#define MAX_PFNCOUNT 10000

struct swaphints_request{
	u32 count;
	u64 pfns[MAX_PFNCOUNT];
} typedef swaphints_request_t;

struct swaphints_response{
	u32 count;
	u64 returncodes[MAX_PFNCOUNT];
} typedef swaphints_response_t;

/* While these are statically sized the 'count' field will enable us to 
 * signal how many elements if the array to process even if the ioctl 
 * transfers the whole list.
 */

#endif /* _SWAPHINTS_H_ */
