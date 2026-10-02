# Kernel module build. Requires kernel headers for the running kernel
# (e.g. apt install linux-headers-$(uname -r)) or a configured kernel tree.
#
# Cross-compile example (adjust ARCH/CROSS_COMPILE for your target):
#   make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- KDIR=/path/to/kernel
#
# The user-space test program builds with the host compiler only.

obj-m += hello_char.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

user:
	$(CC) -Wall -Wextra -Werror -O2 -o hello_test hello_test.c

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f hello_test

.PHONY: all user clean
