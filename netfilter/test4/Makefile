# ========== 内核模块 ==========
obj-m += nf_queue_pkt.o

KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# ========== 用户程序 ==========
CC = gcc
CFLAGS = -Wall -O2
LDFLAGS = -lnetfilter_queue -lnfnetlink
TARGET = nfqueue_userspace
SOURCES = nfqueue_userspace.c

# ========== 目标 ==========
all: kernel userspace

kernel:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

userspace: $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS)

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
	rm -f *.o *.ko *.mod.c *.mod *.symvers *.order .*.cmd .tmp_versions $(TARGET)

install: kernel
	sudo insmod nf_queue_pkt.ko

remove:
	sudo rmmod nf_queue_pkt

run: userspace
	sudo ./$(TARGET)

run-bg: userspace
	sudo killall $(TARGET) 2>/dev/null || true
	sudo ./$(TARGET) > nfqueue.log 2>&1 &

stop:
	sudo killall $(TARGET) 2>/dev/null || true

test: install run-bg
	@echo "=== Sending ping ==="
	ping -c 3 8.8.8.8
	@sleep 1
	@echo "=== Kernel log ==="
	sudo dmesg | grep "Queueing ICMP" | tail -3
	@echo "=== Userspace log ==="
	tail -10 nfqueue.log 2>/dev/null || echo "No log"

test-auto: all
	@echo "=== Auto Test ==="
	sudo insmod nf_queue_pkt.ko 2>/dev/null || true
	sudo killall $(TARGET) 2>/dev/null || true
	sudo ./$(TARGET) &
	sleep 1
	ping -c 3 8.8.8.8 > /dev/null
	sleep 1
	sudo dmesg | grep "Queueing ICMP" | tail -3
	sudo killall $(TARGET) 2>/dev/null || true
	sudo rmmod nf_queue_pkt 2>/dev/null || true
	@echo "=== Done ==="

status:
	@echo "Module: $$(lsmod | grep nf_queue_pkt || echo 'Not loaded')"
	@echo "Program: $$(pgrep -l $(TARGET) || echo 'Not running')"

help:
	@echo "Targets: all, kernel, userspace, clean"
	@echo "         install, remove, run, run-bg, stop"
	@echo "         test, test-auto, status, help"

.PHONY: all kernel userspace clean install remove run run-bg stop test test-auto status help
