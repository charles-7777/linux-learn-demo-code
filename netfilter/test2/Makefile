obj-m += netfilter_ipaddr2.o

KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean

install:
	sudo insmod netfilter_ipaddr2.ko

remove:
	sudo rmmod netfilter_ipaddr2.ko

test:
	sudo dmesg -c
	ping -c 3 8.8.8.8
	curl -s http://www.baidu.com > /dev/null
	sudo dmesg | tail -20

.PHONY: all clean install remove test
