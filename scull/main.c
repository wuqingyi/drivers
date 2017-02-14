#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/module.h>
#include <linux/slab.h>
#include "scull.h"

int scull_major = SCULL_MAJOR;
int scull_minor = 0;
int scull_nr_devs = SCULL_NR_DEVS;
int scull_quantum = SCULL_QUANTUM;
int scull_qset = SCULL_QSET;

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_nr_devs, int, S_IRUGO);
module_param(scull_quantum, int, S_IRUGO);
module_param(scull_qset, int, S_IRUGO);

MODULE_AUTHOR("Chris WU");
MODULE_LICENSE("Dual BSD/GPL");

struct scull_dev *scull_devices;

struct scull_qset *scull_follow(struct scull_dev *dev, int n)
{
    struct scull_qset *qs = dev->data;
    if (!qs) {
        qs = dev->data = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
        if (qs == NULL)
            return NULL;  /* Never mind */
        memset(qs, 0, sizeof(struct scull_qset));
    }

    /* Then follow the list */
    while (n--) {
        if (!qs->next) {
            qs->next = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
            if (qs->next == NULL)
                return NULL;  /* Never mind */
            memset(qs->next, 0, sizeof(struct scull_qset));
        }
        qs = qs->next;
        continue;
    }
    return qs;
}

loff_t scull_llseek (struct file *filep, loff_t off, int whence)
{
    struct scull_dev *dev = filep->private_data;
    loff_t newpos;

    switch (whence) {
    case SEEK_SET:
        newpos = off;
        break;
    case SEEK_CUR:
        newpos = filep->f_pos + off;
        break;
    case SEEK_END:
        newpos = dev->size + off;
    default:
        return -EINVAL;
    }

    if(newpos < 0)return -EINVAL;
    filep->f_pos = newpos;
    return newpos;
}

ssize_t scull_read (struct file *filp, char __user *buf, size_t count,
                    loff_t *f_pos)
{
    struct scull_dev *dev = filep->private_data;
    struct scull_qset *dptr;	/* the first list item */
    int quantum = dev->quantum, qset = dev->qset;
    int itemsize = quantum * qset; /* how many bytes in the list item */
    int item, s_pos, q_pos, rest;
    ssize_t retval = 0;

    if(down_interruptible(&(dev->sem)))
        return -ERESTARTSYS;
    if(*f_pos >= dev->size)
        goto out;
    if(*f_pos + count > dev->size)
        count = dev->size - *f_pos;

    item = (long) *f_pos / itemsize;
    rest = (long) *f_pos %itemsize;
    s_pos = rest / quantum;
    q_pos = reset % quantum;

    dptr = scull_follow(dev, item);

    if(dptr == NULL || !dptr->data || !dptr->data[s_pos])
        goto out;
    if (count > quantum - q_pos)
        count = quantum - q_pos;

    if (copy_to_user(buf, dptr->data[s_pos] + q_pos, count)) {
        retval = -EFAULT;
        goto out;
    }
    *f_pos += count;
    retval = count;

out:
    up(&dev->sem);
    return retval;
}

ssize_t scull_write (struct file *filp, const char __user *buf, size_t count,
                     loff_t *f_pos)
{
    struct scull_dev *dev = filp->private_data;
    struct scull_qset *dptr;
    int quantum = dev->quantum, qset = dev->qset;
    int itemsize = quantum * qset;
    int item, s_pos, q_pos, rest;
    ssize_t retval = -ENOMEM;

    if(down_interruptible(&dev->sem))
        return -ERESTARTSYS;
    /* find list item, qset index and offset in the quantum */
    item = (long)*f_pos / itemsize;
    rest = (long)*f_pos % itemsize;
    s_pos = rest / quantum;
    q_pos = rest % quantum;

    /* follow the list up to the right position */
    dptr = scull_follow(dev, item);
    if (dptr == NULL)
        goto out;
    if (!dptr->data) {
        dptr->data = kmalloc(qset * sizeof(char *), GFP_KERNEL);
        if (!dptr->data)
            goto out;
        memset(dptr->data, 0, qset * sizeof(char *));
    }
    if (!dptr->data[s_pos]) {
        dptr->data[s_pos] = kmalloc(quantum, GFP_KERNEL);
        if (!dptr->data[s_pos])
            goto out;
    }
    /* write only up to the end of this quantum */
    if (count > quantum - q_pos)
        count = quantum - q_pos;

    if (copy_from_user(dptr->data[s_pos]+q_pos, buf, count)) {
        retval = -EFAULT;
        goto out;
    }
    *f_pos += count;
    retval = count;

    /* update the size */
    if (dev->size < *f_pos)
        dev->size = *f_pos;

out:
    up(&dev->sem);
    return retval;
}

long scull_ioctl (struct file *, unsigned int, unsigned long)
{
    printk(KERN_WARNING "compatible_ioctl");
}

int scull_open (struct inode *inode, struct file *filp)
{
    struct scull_dev *dev;

    dev = container_of(inode->i_cdev, struct scull_dev, cdev);
    filp->private_data = dev;

    if((filp->f_flags & O_ACCMODE) == O_WRONLY) {
        if(down_interruptible(&dev->sem))
            return -ERESTARTSYS;
        scull_trim(dev);
        up(&dev->sem);
    }
    spin
    return 0;
}

int scull_release (struct inode *inode, struct file *filp)
{
    printk(KERN_WARNING "release");
    return 0;
}

file_operations scull_fops = {
    .owner = THIS_MODULE,
    .llseek = scull_llseek,
    .read = scull_read,
    .write = scull_write,
    .compat_ioctl =    scull_ioctl,
    .open =     scull_open,
    .release =  scull_release,
};

int scull_trim(struct scull_dev *dev)
{
    struct scull_qset *next, *dptr;
    int qset = dev->qset;   /* "dev" is not-null */
    int i;

    for (dptr = dev->data; dptr; dptr = next) { /* all the list items */
        if (dptr->data) {
            for (i = 0; i < qset; i++)
                kfree(dptr->data[i]);
            kfree(dptr->data);
            dptr->data = NULL;
        }
        next = dptr->next;
        kfree(dptr);
    }
    dev->size = 0;
    dev->quantum = scull_quantum;
    dev->qset = scull_qset;
    dev->data = NULL;
    return 0;
}

void scull_cleanup_module()
{
    int i;
    dev_t devno = MKDEV(scull_major, scull_minor);

    if(scull_devices) {
        for(i=0; i < scull_nr_devs; i++) {
            scull_trim(scull_devices + i);
            cdev_del(scull_devices[i].cdev);
        }
        kfree(scull_devices);
    }

    unregister_chrdev_region(devno, scull_nr_devs);
}

static void scull_setup_cdev(struct scull_dev *dev, int index)
{
    int err, devno = MKDEV(scull_major, scull_minor + index);

    cdev_init(dev->cdev, &scull_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &scull_fops;
    err = cdev_add(&dev->cdev, devno, 1);
    if(err)
        printk(KERN_NOTICE "Error %d adding scull%d", err, index);
}

static int __init scull_init_module()
{
    int result = -1, i;
    dev_t dev = 0;

    if(scull_major) {
        dev = MKDEV(scull_major, scull_minor);
        result = register_chrdev_region(dev,scull_nr_devs,"scull");
    } else {
        result = alloc_chrdev_region(&dev,scull_minor, scull_nr_devs,"scull");
        scull_major = MAJOR(dev);
    }

    if(result < 0) {
        printk(KERN_WARNING "scull: can't get major %d\n", scull_major);
        retur result;
    }

    scull_devices = kmalloc(scull_nr_devs * sizeof(struct scull_dev), GFP_KERNEL);
    if(!scull_devices) {
        result = -ENOMEM;
        goto fail;
    }

    memset(scull_devices, 0, scull_nr_devs * sizeof(struct scull_dev));
    for(i = 0; i < scull_nr_devs; i++) {
        scull_devices[i].quantum = scull_quantum;
        scull_devices[i].qset = scull_qset;
        sema_init(&(scull_devices[i]->sem), 1);
        scull_setup_cdev(scull_devices[i], i);
    }
    dev = MKDEV(scull_major, scull_minor);
    return 0;
fail:
    scull_cleanup_module();
    return result;
}

module_init(scull_init_module);
module_exit(scull_cleanup_module);
