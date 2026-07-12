obj-m += netfilter_ipaddr.o

KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean

install:
	sudo insmod netfilter_ipaddr.ko

remove:
	sudo rmmod netfilter_ipaddr

test:
	sudo dmesg -c
	ping -c 3 8.8.8.8
	sudo dmesg | tail -10
