# Basic Makefile to pull in kernel's KBuild to build an out-of-tree
# kernel module

KDIR ?= /lib/modules/$(shell uname -r)/build

all: modules babbler2-test

babbler2-test: babbler2-test.c
	gcc --std=c99 -Wall -O2 -pthread -o $@ $^

xt_filter.ko modules:
	$(MAKE) -C $(KDIR) M=$$PWD $@

clean:
	$(MAKE) -C $(KDIR) M=$$PWD $@
	-rm babbler2-test
