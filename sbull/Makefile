ifneq ($(KERNELRELEASE),)
# call from kernel build system
#pipe.o access.o
sbull-objs := main.o
obj-m	:= sbull.o

else

KERNELDIR ?= /home/wuqingyi/linux-3.16
PWD       := $(shell pwd)

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

endif



clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions

depend .depend dep:
	$(CC) $(CFLAGS) -M *.c > .depend


ifeq (.depend,$(wildcard .depend))
include .depend
endif
