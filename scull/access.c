#include <linux/kernel.h> //printk()
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>   //kmalloc()
#include <linux/atomic.h>
#include <asm-generic/atomic.h>
#include <uapi/asm-generic/errno-base.h>
#include <uapi/asm-generic/fcntl.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <asm-generic/spinlock.h>

static dev_t scull_s_fristdev;

static struct scull_dev scull_s_device;
static atomic_t scull_s_available = ATOMIC_INIT(1);

static int scull_s_open(struct inode *inode, struct file *filp)
{
    struct scull_dev *dev = &scull_s_fristdev;

    if(!atomic_dec_and_test(&scull_s_available)) {
        atomic_inc(&scull_s_available);
        return -EBUSY;
    }

    if((filp->f_flags & O_ACCMODE) == O_WRONLY)
        scull_trim(dev);
    filp->private_data = dev;
    return 0;
}

static int scull_s_release(struct inode *inode, struct file *filp)
{
    atomic_inc(&scull_s_available);
    return 0;
}


static struct scull_dev scull_u_device;
static int scull_u_count;
static uid_t scull_u_owner;
static spinlock_t scull_u_lock;
spin_lock_init(scull_u_lock);

static int scull_u_open(struct inode *inode, struct file *filp)
{
    struct scull_dev *dev = &scull_u_device;

    spin_lock(&scull_u_lock);
    if(scull_u_count &&
       (scull_u_owner != current->uid) &&
       (scull_u_owner != current->euid) &&
       !capable(CAP_DAC_OVERRIDE)) {
        spin_unlock(& scull_u_lock);
        return -EBUSY;
    }

    if(scull_u_count == 0)
        scull_u_owner = current->uid;

    scull_u_count++;
    spin_unlock(&scull_u_lock);

    if((filp->f_flags & O_ACCMODE) == O_WRONLY)
        scull_trim(dev);
    filp->private_data = dev;
    return 0;
}

static int scull_u_release(stuct inode *inode, struct file *filp)
{
    spin_lock(&scull_u_lock);
    scull_u_count--;
    spin_unlock(&scull_u_lock);
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////
struct file_operations scull_sngl_fops = {
    .owner =	THIS_MODULE,
    .llseek =     	scull_llseek,
    .read =       	scull_read,
    .write =      	scull_write,
    .ioctl =      	scull_ioctl,
    .open =       	scull_s_open,
    .release =    	scull_s_release,
};
