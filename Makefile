ifneq ($(KERNELRELEASE),)
# kbuild

-include $(M)/.config

obj-m	:= frib_bpm.o

frib_bpm-objs := bpm-main.o

else
# user makefile

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all: modules

modules_install modules help clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) $@

.PHONY: all modules_install modules clean

endif
