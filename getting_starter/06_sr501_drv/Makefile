KERN_DIR = /home/zyb/Desktop/vscode-ws/Linux-4.9.88

all:
	make -C $(KERN_DIR) M=`pwd` modules
	$(CROSS_COMPILE)gcc -o sr501_test sr501_test.c

clean:
	make -C $(KERN_DIR) M=`pwd` modules clean
	rm -rf modules.order
	rm -f sr501_test

obj-m	+= sr501_drv.o