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

/* start of swaphints structs */
#define MAX_PFNCOUNT 1000

typedef struct swaphints_request{
	uint32_t count;
	uint64_t pfns[MAX_PFNCOUNT];
} swaphints_request_t;

typedef struct swaphints_response{
	uint32_t count;
	uint64_t returncodes[MAX_PFNCOUNT];
} swaphints_response_t;

/* While these are statically sized the 'count' field will enable us to 
 * signal how many elements if the array to process even if the ioctl 
 * transfers the whole list.
 */

/*
 * Ioctl definitions
 */

/* Use 'k' as magic number */
#define SWAPHINTS_IOC_MAGIC  'k'
/* Please use a different 8-bit number in your code */
/* refer to documentation/ioctl-number.txt */

#define SWAPHINTS_IOCRESET     _IO(SWAPHINTS_IOC_MAGIC, 0)
#define SWAPHINTS_SEND_REQUEST _IOW(SWAPHINTS_IOC_MAGIC,  1, int)
#define SWAPHINTS_GET_RESPONSE _IOR(SWAPHINTS_IOC_MAGIC,  2, int)
#define SWAPHINTS_IOC_MAXNR 2

#ifdef __KERNEL__

#ifndef SWAPHINTS_MAJOR
#define SWAPHINTS_MAJOR 0   /* dynamic major by default */
#endif


long swaphints_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

struct swaphints_dev {
	swaphints_request_t requests;	/* requests being sent down */
	swaphints_response_t responses;	/* responses being returned */
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
//extern int swaphints_nr_devs;

#endif /* __KERNEL__ */

#endif /* _SWAPHINTS_H_ */
