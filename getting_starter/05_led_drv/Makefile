KERN_DIR = /home/zyb/Desktop/vscode-ws/Linux-4.9.88

all:
	make -C $(KERN_DIR) M=`pwd` modules
	$(CROSS_COMPILE)gcc -o led_test led_test.c

clean:
	make -C $(KERN_DIR) M=`pwd` modules clean
	rm -rf modules.order
	rm -f led_test

obj-m	+= led_drv.o