# Comment/uncomment the following line to disable/enable debugging
DEBUG = y

ifeq ($(DEBUG),y)
  ccflags-y += -g -DSNULL_DEBUG
endif

obj-m := snull.o

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
