#include "linux/if.h"
#include "linux/kdev_t.h"
#include "linux/stddef.h"
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/mman.h>
#include <linux/module.h>

#include <linux/uaccess.h>

// 自动生成主设备号
unsigned int hello_drv_transfer_major;
// 内核空间buf
static unsigned char hello_buf[100];
// 创建class
static struct class *hello_transfer_data_class;

/* 2.实现fops结构体中的函数 */
/*
 * 打开文件获取文件节点句柄
 */
static int transfer_open(struct inode *node, struct file *filp) {
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
    return 0;
}
/*
 * 读取文件filp的数据，将其写入buf空间
 */
static ssize_t transfer_read(struct file *filp, char __user *buf, size_t size, loff_t *offset) {

  // 驱动程序能力：每次仅读取size=100的数据
    unsigned long len = size > 100 ? 100 : size;

    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
    copy_to_user(buf, hello_buf, len);
    return 0;
}
/*
 * 将buf空间中的数据写入文件filp
 */
static ssize_t transfer_write(struct file *filp, const char __user *buf, size_t size, loff_t *offset) {
    // 驱动程序能力：每次仅写入size=100的数据至内核
    unsigned long len = size > 100 ? 100 : size;
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
    copy_from_user(hello_buf, buf, len);
    return 0;
}
/*
 * 关闭文件句柄
 */
static int transfer_close(struct inode *node, struct file *filp) {
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
    return 0;
}

/* 1.创建fops结构体 */
static const struct file_operations hello_drv_transfer_data_ops = {
    .owner = THIS_MODULE,
    .open = transfer_open,
    .read = transfer_read,
    .write = transfer_write,
    .release = transfer_close,
};

/* 3.入口函数注册fops结构体 */
static int hello_transfer_init(void) {
    hello_drv_transfer_major = register_chrdev(0, "hello_drv_transfer_data", &hello_drv_transfer_data_ops);

    // 创建class
    hello_transfer_data_class = class_create(THIS_MODULE, "hello_transfer_data_class");
    if (IS_ERR(hello_transfer_data_class)) {
        pr_warn("Unable to create backlight class; errno = %ld\n", PTR_ERR(hello_transfer_data_class));
        return PTR_ERR(hello_transfer_data_class);
    }

    /*
     * 创建device，后台会创建一个/dev/hello_drv_transfer_device的设备节点
     * 主设备号为hello_drv_transfer_major，次设备号为0
     */
    device_create(hello_transfer_data_class, NULL, MKDEV(hello_drv_transfer_major, 0), NULL, "hello_drv_transfer_device");
}

/* 4.出口函数注销fops结构体 */
static void hello_transfer_exit(void) {
    device_destroy(hello_transfer_data_class, MKDEV(hello_drv_transfer_major, 0));
    class_destroy(hello_transfer_data_class);
    unregister_chrdev(hello_drv_transfer_major, "hello_drv_transfer_data");
}

/* 5.其它信息 */
module_init(hello_transfer_init);
module_exit(hello_transfer_exit);
MODULE_LICENSE("GPL");