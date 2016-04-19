# Basic Makefile to pull in kernel's KBuild to build an out-of-tree
# kernel module

KDIR ?= /lib/modules/`uname -r`/build

all: modules babbler-test

babbler-test: babbler-test.c
	gcc --std=c99 -Wall -O2 -pthread -o $@ $^

modules:
	$(MAKE) -C $(KDIR) M=$$PWD $@

clean:
	$(MAKE) -C $(KDIR) M=$$PWD $@
	-rm babbler-test
