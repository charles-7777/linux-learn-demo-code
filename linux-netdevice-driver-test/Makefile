# Build two modules:
#   dummy-hw.ko  - simulated hardware backend (exports lt_hw_tx / lt_request_irq)
#   dummy-eth.ko - the virtual ethernet netdevice (depends on dummy-hw)
obj-m += dummy-hw.o
obj-m += dummy-eth.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
