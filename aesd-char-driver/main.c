/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>

#include "aesdchar.h"
#include "aesd-circular-buffer.h"
#include "aesd_ioctl.h"


int aesd_major =   1; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Eduard Grueso");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    
    filp->private_data = container_of(inode->i_cdev, struct aesd_dev, cdev);

    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry *entry;
    size_t entry_offset_byte_rtn;
    size_t bytes_to_copy;
    ssize_t retval = 0;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    entry = aesd_circular_buffer_find_entry_offset_for_fpos(
        &dev->buffer,
        *f_pos,
        &entry_offset_byte_rtn);

    if (!entry) {
        retval = 0;
        goto out;
    }

    bytes_to_copy = entry->size - entry_offset_byte_rtn;
    if (bytes_to_copy > count)
        bytes_to_copy = count;

    if (copy_to_user(buf, entry->buffptr + entry_offset_byte_rtn, bytes_to_copy)) {
        retval = -EFAULT;
        goto out;
    }

    *f_pos += bytes_to_copy;
    retval = bytes_to_copy;

out:
    mutex_unlock(&dev->lock);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
     struct aesd_dev *dev = filp->private_data;
    char *new_pending;
    size_t new_pending_size;
    char *newline;
    struct aesd_buffer_entry entry;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    new_pending_size = dev->pending_write_size + count;
    new_pending = kmalloc(new_pending_size, GFP_KERNEL);
    if (!new_pending) {
        mutex_unlock(&dev->lock);
        return -ENOMEM;
    }

    if (dev->pending_write) {
        memcpy(new_pending, dev->pending_write, dev->pending_write_size);
        kfree(dev->pending_write);
    }

    if (copy_from_user(new_pending + dev->pending_write_size, buf, count)) {
        kfree(new_pending);
        mutex_unlock(&dev->lock);
        return -EFAULT;
    }

    dev->pending_write = new_pending;
    dev->pending_write_size = new_pending_size;

    newline = memchr(dev->pending_write, '\n', dev->pending_write_size);
    if (!newline) {
        mutex_unlock(&dev->lock);
        return count;
    }

    entry.size = (newline - dev->pending_write) + 1;
    entry.buffptr = kmalloc(entry.size, GFP_KERNEL);
    if (!entry.buffptr) {
        mutex_unlock(&dev->lock);
        return -ENOMEM;
    }
    memcpy((char *)entry.buffptr, dev->pending_write, entry.size);

    aesd_circular_buffer_add_entry(&dev->buffer, &entry);

    if (entry.size < dev->pending_write_size) {
        size_t remaining = dev->pending_write_size - entry.size;
        char *remaining_buf = kmalloc(remaining, GFP_KERNEL);
        memcpy(remaining_buf, dev->pending_write + entry.size, remaining);
        kfree(dev->pending_write);

        dev->pending_write = remaining_buf;
        dev->pending_write_size = remaining;
    } else {
        kfree(dev->pending_write);
        dev->pending_write = NULL;
        dev->pending_write_size = 0;
    }

    mutex_unlock(&dev->lock);
    return count;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
    .llseek =   aesd_llseek,
    .unlocked_ioctl = aesd_unlocked_ioctl,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t devno;
    int result = alloc_chrdev_region(&devno, aesd_minor, 1, "aesdchar");
    if (result < 0)
        return result;
    aesd_major = MAJOR(devno);

    memset(&aesd_device, 0, sizeof(aesd_device));
    mutex_init(&aesd_device.lock);
    aesd_circular_buffer_init(&aesd_device.buffer);

    result = aesd_setup_cdev(&aesd_device);
    if (result)
        unregister_chrdev_region(devno, 1);

    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    mutex_lock(&aesd_device.lock);
    if (aesd_device.pending_write) {
        kfree(aesd_device.pending_write);
    }

    {
        uint8_t idx;
        struct aesd_buffer_entry *entry;
        AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.buffer, idx) {
            kfree((void *)entry->buffptr);
        }
    }
    mutex_unlock(&aesd_device.lock);

    unregister_chrdev_region(devno, 1);
}

loff_t aesd_llseek(struct file *filp, loff_t offset, int whence)
{
    struct aesd_dev *dev = filp->private_data;
    size_t total_bytes = 0;
    size_t i;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    for (i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++) {
        if (!dev->buffer.entry[i].buffptr)
            break;
        total_bytes += dev->buffer.entry[i].size;
    }

    mutex_unlock(&dev->lock);

    switch (whence) {
        case SEEK_SET:
            if (offset < 0 || offset > total_bytes)
                return -EINVAL;
            filp->f_pos = offset;
            break;
        case SEEK_CUR:
            if ((filp->f_pos + offset) < 0 ||
                (filp->f_pos + offset) > total_bytes)
                return -EINVAL;
            filp->f_pos += offset;
            break;
        case SEEK_END:
            if (offset > 0 || ((loff_t)total_bytes + offset) < 0)
                return -EINVAL;
            filp->f_pos = total_bytes + offset;
            break;
        default:
            return -EINVAL;
    }

    return filp->f_pos;
}

long aesd_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct aesd_seekto seekto;
    struct aesd_dev *dev = filp->private_data;
    size_t write_count = 0;
    size_t cumulative = 0;
    size_t i;

    if (_IOC_TYPE(cmd) != AESD_IOC_MAGIC || _IOC_NR(cmd) > AESDCHAR_IOC_MAXNR)
        return -EINVAL;

    if (copy_from_user(&seekto, (void __user *)arg, sizeof(seekto)))
        return -EFAULT;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    for (i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++) {
        if (!dev->buffer.entry[i].buffptr)
            break;
        write_count++;
    }

    if (seekto.write_cmd >= write_count) {
        mutex_unlock(&dev->lock);
        return -EINVAL;
    }

    for (i = 0; i < seekto.write_cmd; i++)
        cumulative += dev->buffer.entry[i].size;

    if (seekto.write_cmd_offset >= dev->buffer.entry[seekto.write_cmd].size) {
        mutex_unlock(&dev->lock);
        return -EINVAL;
    }

    filp->f_pos = cumulative + seekto.write_cmd_offset;

    mutex_unlock(&dev->lock);
    return 0;
}


module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
