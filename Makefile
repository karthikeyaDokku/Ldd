obj-m += myCameraDriver.o
ccflags-y += -DCONFIG_KUNIT=1

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

help:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) help
