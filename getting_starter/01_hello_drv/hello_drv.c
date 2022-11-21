#include <linux/mm.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mman.h>
#include <linux/random.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/raw.h>
#include <linux/tty.h>
#include <linux/capability.h>
#include <linux/ptrace.h>
#include <linux/device.h>
#include <linux/highmem.h>
#include <linux/backing-dev.h>
#include <linux/shmem_fs.h>
#include <linux/splice.h>
#include <linux/pfn.h>
#include <linux/export.h>
#include <linux/io.h>
#include <linux/uio.h>
#include <linux/fs.h>

#include <linux/uaccess.h>

// 定义主设备号
static int hello_major;

// 不想暴露、污染命名空间，使用static即可
static int hello_open (struct inode *node, struct file *filp)
{
    // 打印格式，哪一个文件的哪一个函数的哪一行
    printk("%s %s %d\n",__FILE__,__FUNCTION__,__LINE__);
    return 0;
}

/*
* param：file(哪个文件)、buf(buf空间)、size(文件大小)、offset(偏移值，一般不用)
*/
static ssize_t hello_read (struct file *filp, char __user *buf, size_t size, loff_t *offset)
{
    printk("%s %s %d\n",__FILE__,__FUNCTION__,__LINE__);
    return 0;
}

/*
*
*/
static ssize_t hello_write (struct file *filp, const char __user *buf, size_t size, loff_t *offset) {
    printk("%s %s %d\n",__FILE__,__FUNCTION__,__LINE__);
    return 0;
}

/*
*
*/
static int hello_release (struct inode *node, struct file *filp) {
    printk("%s %s %d\n",__FILE__,__FUNCTION__,__LINE__);
    return 0;
}

/* 1. create file operations    创建file_operations结构体 */
static const struct file_operations hello_drv_fops = {
    .owner      = THIS_MODULE,
    .open       = hello_open,
    .read       = hello_read,
    .write      = hello_write,
    .release    = hello_release,
};

/* 2. register_chrdev          使用register_chrdev注册结构体 */

/* 3. entry_function            在入口函数处注册结构体 */
static int hello_init(void)
{
    hello_major = register_chrdev(0, "hello_drv", &hello_drv_fops);
    return 0;
}

/* 4. exit_entry                出口函数卸载驱动 */
static void hello_exit(void)
{
    unregister_chrdev(hello_major, "hello_drv");
}

module_init(hello_init);
module_exit(hello_exit);
MODULE_LICENSE("GPL");