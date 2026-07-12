obj-m += nf_blockpkt.o

KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean

install:
	sudo insmod nf_blockpkt.ko

remove:
	sudo rmmod nf_blockpkt

test:
	@echo "=== Testing ICMP blocking ==="
	sudo dmesg -c
	@echo "Pinging 8.8.8.8 (should be blocked)..."
	ping -c 3 8.8.8.8 || true
	@echo ""
	@echo "Pinging localhost (should be blocked)..."
	ping -c 3 127.0.0.1 || true
	@echo ""
	@echo "=== Kernel messages ==="
	sudo dmesg | tail -10

info:
	@echo "Module status:"
	lsmod | grep nf_blockpkt || echo "Module not loaded"

.PHONY: all clean install remove test info
