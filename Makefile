obj-m += sdrv.o
sdrv-y += sdrv_scmd_handler.o sdrv_host_dev.o sdrv_ioctl.o sdrv_main.o

KERNELBUILD := /lib/modules/`uname -r`/build

all: modules

default modules:
	make -C $(KERNELBUILD) M=$(shell pwd) modules
	rm -rf *.o *.ko.unsigned
	rm -rf .depend .*.cmd *.mod.c .tmp_versions *.symvers .*.d *.markers *.order

clean:
	rm -rf *.o *.ko *.ko.unsigned
	rm -rf .depend .*.cmd *.mod.c .tmp_versions *.symvers .*.d *.markers *.order

