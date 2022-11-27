KERN_DIR = /home/zyb/Desktop/vscode-ws/Linux-4.9.88

all:
	make -C $(KERN_DIR) M=`pwd` modules
	$(CROSS_COMPILE)gcc -o hello_test hello_test.c

clean:
	make -C $(KERN_DIR) M=`pwd` modules clean
	rm -rf modules.order
	rm -f hello_test

obj-m	+= hello_drv_transfer_data.o