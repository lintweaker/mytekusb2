obj-m += snd-usb-mytek.o
snd-usb-mytek-objs += chip.o comm.o control.o firmware.o pcm.o

FW_PATH=/lib/firmware
FW_MYTEK_PATH=$(FW_PATH)/mytek

all:
	make CONFIG_DEBUG_SECTION_MISMATCH=y -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

install:
	rm -f /lib/modules/$(shell uname -r)/extras/snd-usb-mytek.ko
	rm -f /lib/modules/$(shell uname -r)/extra/snd-usb-mytek.ko
	mkdir -p /lib/modules/$(shell uname -r)/extra
	cp snd-usb-mytek.ko /lib/modules/$(shell uname -r)/extra
	depmod -a

uninstall:
	rm -f /lib/modules/$(shell uname -r)/extra/snd-usb-mytek.ko

kremove:
	for x in $(shell find /lib/modules/$(shell uname -r) -iname snd-usb-mytek\*); do \
	rm -f $$x; \
	done

