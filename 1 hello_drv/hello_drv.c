#include <linux/module.h>

#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/tty.h>
#include <linux/kmod.h>
#include <linux/gfp.h>


// 定义一个buffer
static char kernel_buffer[1024];
// 定义一个取最小值的三目算法
#define MIN(a,b) (a < b ? a : b)
// 自动创建设备主设备号所需结构体
static struct class *hello_class;


/* 1.确定主设备号 */
static int major = 0;  // 主设备号=节点句柄，赋值为0代表让内核自主帮我们创建
static int err = 0;


/* 
 * 3.实现对应的open/read/write函数，填入file_operations结构体中
 *   之所以移动到fops前面，是因为fops用到了这些函数，懒得声明了，正常放在.h文件中
 */
/* 3.实现对应的open/read/write函数，填入file_operations结构体中 */
int hello_drv_open (struct inode *node, struct file *file) {
    printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    return 0;
}
ssize_t hello_drv_read (struct file *file, char __user *buf, size_t size, loff_t * offset) {
    printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    copy_to_user(buf, kernel_buffer, MIN(size, 1024));
    return MIN(size, 1024);
}
ssize_t hello_drv_write (struct file *file, const char __user *buf, size_t size, loff_t *offset) {
    printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    copy_from_user(kernel_buffer, buf, MIN(size, 1024));
    return MIN(size, 1024);
}
int hello_drv_release (struct inode *node, struct file *file) {
    printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    return 0;
}


/* 2.定义自己的file_operations结构体 */
static struct file_operations hello_drv_ops {
    .owner		= THIS_MODULE,
    .open       = hello_drv_open,
    .read       = hello_drv_read,
    .write      = hello_drv_write,
    .release    = hello_drv_release,
};


/* 4.把file_operations结构体告诉内核：注册驱动程序 */
static int __init hello_drv_init(void) {
    printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    major = register_chrdev(0, "hello", &hello_drv_ops);

    /* 
     * 自动创建设备节点
     *  1st 创建一个类class
     *  2nd 创建一个device
     */
    hello_class = class_create(THIS_MODULE, "hello_class");
    err = PTR_ERR(hello_class);
    if (IS_ERR(hello_class)) {
        unregister_chrdev(major, "hello");
        return -1;
    }

    /* 以后就可以使用/dev/hello来访问驱动程序了 */
    device_create(hello_class, NULL, MKDEV(major, 0), NULL, "hello");    // 以后就可以使用/dev/hello来访问驱动程序了
    
    return 0;
}

/* 6.有入口函数就应该有出口函数：卸载驱动程序时调用 */
static void __exit hello_drv_exit(void) {
    printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    device_unregister(hello_class, MKDEV(major, 0));
    class_destroy(hello_class);
    unregister_chrdev(major, "hello");
    return 0;
}

/* 5.谁来注册驱动程序？需要有一个入口函数；安装驱动程序时就会去调用这个入口函数 */
module_init(hello_drv_init);
module_exit(hello_drv_exit);

/* 7.其它完善：提供设备信息 */
MODULE_LICENSE("GPL");