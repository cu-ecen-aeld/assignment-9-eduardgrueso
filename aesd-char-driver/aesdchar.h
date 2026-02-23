/*
 * aesdchar.h
 *
 *  Created on: Oct 23, 2019
 *      Author: Dan Walkes
 */

#include "aesd-circular-buffer.h"

#ifndef AESD_CHAR_DRIVER_AESDCHAR_H_
#define AESD_CHAR_DRIVER_AESDCHAR_H_

#define AESD_DEBUG 1  //Remove comment on this line to enable debug

#undef PDEBUG             /* undef it, just in case */
#ifdef AESD_DEBUG
#  ifdef __KERNEL__
     /* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_DEBUG "aesdchar: " fmt, ## args)
#  else
     /* This one for user space */
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif
#else
#  define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif

struct aesd_dev
{
    struct cdev cdev;                            
    struct mutex lock;                              
    struct aesd_circular_buffer buffer;           
    char *pending_write;                            
    size_t pending_write_size; 
};

loff_t aesd_llseek(struct file *filp, loff_t offset, int whence);
long aesd_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

#endif /* AESD_CHAR_DRIVER_AESDCHAR_H_ */
