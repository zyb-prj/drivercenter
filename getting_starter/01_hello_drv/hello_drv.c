#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mman.h>
#include <linux/random.h>
#include <linux/init.h>
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

#include <linux/uaccess.h>

// 不想暴露、污染命名空间，使用static即可
static int hello_open (struct inode *node, struct file *file)
{
    // 打印格式，哪一个文件的哪一个函数的哪一行
    printk("%s %s %d\n",__FILE__,__FUNCTION__,__LINE__);
    return 0;
}

/*
* param：file(哪个文件)、buf(buf空间)、size(文件大小)、offset(偏移值，一般不用)
*/
static ssize_t hello_read (struct file *file, char __user *buf, size_t size, loff_t *offset)
{
    
}

/* 1. create file operations    创建file_operations结构体 */
static const struct file_operations hello_drv_fops = {
    .owner      = THIS_MODULE,
    .read       = hello_read,
    .write      = hello_write,
    .open       = hello_open,
};

/* 2. register_chrdev          使用register_chrdev注册结构体 */

/* 3. entry_function            在入口函数处注册结构体 */

/* 4. exit_entry                出口函数卸载驱动 */