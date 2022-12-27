KERN_DIR = /home/zyb/Desktop/vscode-ws/Linux-4.9.88

all:
	make -C $(KERN_DIR) M=`pwd` modules
	$(CROSS_COMPILE)gcc -o template_test sr04_test.c

clean:
	make -C $(KERN_DIR) M=`pwd` modules clean
	rm -rf modules.order
	rm -f sr04_test

obj-m	+= sr04_drv.o