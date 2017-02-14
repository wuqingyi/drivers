#ifndef SCULL_H_INCLUDED
#define SCULL_H_INCLUDED

#include <linux/semaphore.h>
#include <linux/cdev.h>

#ifndef SCULL_MAJOR
#define SCULL_MAJOR 0
#endif // SCULL_MAJOR

#ifndef SCULL_NR_DEVS
#define SCULL_NR_DEVS 4
#endif // SCULL_NR_DEVS

#ifndef SCULL_QUANTUM
#define SCULL_QUANTUM 4000
#endif // SCULL_QUANTUM

#ifndef SCULL_QSET
#define SCULL_QSET    1000
#endif // SCULL_QSET

struct scull_qset {
    void **data;
    struct scull_qset *next;
};

struct scull_dev {
    struct scull_qset *data;
    int quantum;
    int qset;
    unsigned long size;
    unsigned int access_key;
    struct semaphore sem;
    struct cdev cdev;
};

#endif // SCULL_H_INCLUDED
