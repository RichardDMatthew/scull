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
#include <linux/types.h> /* needed for a common definition of uints */

#ifndef SWAPHINTS_MAJOR
#define SWAPHINTS_MAJOR 0   /* dynamic major by default */
#endif

#ifndef SWAPHINTS_NR_DEVS
#define SWAPHINTS_NR_DEVS 1    /* swaphints0 through swaphints3 */
#endif

#ifndef SWAPHINTS_P_NR_DEVS
#define SWAPHINTS_P_NR_DEVS 1  /* swaphintspipe0 through swaphintspipe3 */
#endif

/* start of swaphints structs */
#define MAX_PFNCOUNT 10

typedef struct swaphints_request{
	u32 count;
	u64 pfns[MAX_PFNCOUNT];
} swaphints_request_t;

typedef struct swaphints_response{
	u32 count;
	u64 returncodes[MAX_PFNCOUNT];
} swaphints_response_t;

/* While these are statically sized the 'count' field will enable us to 
 * signal how many elements if the array to process even if the ioctl 
 * transfers the whole list.
 */

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
extern int swaphints_nr_devs;

long swaphints_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

/*
 * Ioctl definitions
 */

/* Use 'k' as magic number */
#define SWAPHINTS_IOC_MAGIC  'k'
/* Please use a different 8-bit number in your code */

#define SWAPHINTS_IOCRESET     _IO(SWAPHINTS_IOC_MAGIC, 0)
#define SWAPHINTS_SEND_REQUEST _IOW(SWAPHINTS_IOC_MAGIC,  1, int)
#define SWAPHINTS_GET_RESPONSE _IOW(SWAPHINTS_IOC_MAGIC,  2, int)
#define SWAPHINTS_IOC_MAXNR 2

#endif /* _SWAPHINTS_H_ */
