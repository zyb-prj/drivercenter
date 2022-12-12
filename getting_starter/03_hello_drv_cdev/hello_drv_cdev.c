#include "linux/cdev.h"
#include "linux/fs.h"
#include <linux/backing-dev.h>
#include <linux/capability.h>
#include <linux/device.h>
#include <linux/export.h>
#include <linux/highmem.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/module.h>
#include <linux/pfn.h>
#include <linux/ptrace.h>
#include <linux/random.h>
#include <linux/raw.h>
#include <linux/shmem_fs.h>
#include <linux/slab.h>
#include <linux/splice.h>
#include <linux/tty.h>
#include <linux/uio.h>
#include <linux/vmalloc.h>

#include <linux/uaccess.h>

// 内核空间buf
static unsigned char hello_buf[100];
// 创建cdev，分配主次设备号区间
static struct cdev hello_drv_cdev;
// 保存主、次设备号信息；dev中，高20位就是主设备号
static dev_t dev;

// 创建class
static struct class *hello_drv_cdev_class;

/* 2.实现结构体函数 */
/*
 * 打开文件filp返回文件节点句柄
 */
static int cdev_open(struct inode *node, struct file *filp) {
  printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
  return 0;
}
/*
 * 读取文件filp的数据，将其写入用户buf空间
 */
static ssize_t cdev_read(struct file *filp, char __user *buf, size_t size,
                         loff_t *offset) {
  // 驱动程序能力：每次仅读取size=100的数据
  unsigned long len = size > 100 ? 100 : size;
  printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
  copy_to_user(buf, hello_buf, len);
  return len;
}
/*
 * 将用户buf空间中的数据写入文件filp
 */
static ssize_t cdev_write(struct file *filp, const char __user *buf,
                          size_t size, loff_t *offset) {
  // 驱动程序能力：每次仅写入size=100的数据至内核
  unsigned long len = size > 100 ? 100 : size;
  printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
  copy_from_user(hello_buf, buf, len);
  return len;
}
/*
 * 关闭文件句柄
 */
static int cdev_close(struct inode *node, struct file *filp) {
  printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
  return 0;
}

/* 1.构建file_operations结构体 */
static struct file_operations hello_drv_cdev_ops = {
    .owner = THIS_MODULE,
    .open = cdev_open,
    .read = cdev_read,
    .write = cdev_write,
    .release = cdev_close,
};

/* 3.入口函数注册注册file_operations结构体 */
static int hello_cdev_init(void) {

  int ret;

  /*
   * func：申请主、次设备号的名单：
   * param1: dev为返回参数（主设备号）
   * param2：次设备号from（范围起始值）
   * param3：次设备个数
   * param4：名字
   */
  ret = alloc_chrdev_region(&dev, 0, 1, "hello_drv_cdev");
  if (ret < 0) {
    printk(KERN_ERR "alloc_chrdev_region() failed for dev\n");
    return -EINVAL;
  }

  // 初始化字符设备
  cdev_init(&hello_drv_cdev, &hello_drv_cdev_ops);
  // 添加cdev设备信息，主次设备号信息，仅需1个次设备
  ret = cdev_add(&hello_drv_cdev, dev, 1);
  if (ret) {
    printk(KERN_ERR "cdev_add() failed for dev\n");
    return -EINVAL;
  }

  // 让系统自主分配一个设备节点：class-->device
  // 创建class
  hello_drv_cdev_class = class_create(THIS_MODULE, "hello_drv_cdev_class");
  if (IS_ERR(hello_drv_cdev_class)) {
    pr_warn("Unable to create backlight class; errno = %ld\n",
            PTR_ERR(hello_drv_cdev_class));
    return PTR_ERR(hello_drv_cdev_class);
  }
  // 创建device，如下操作会让系统自动生成一个
  // /dev/hello_drv_cdev_device的设备节点
  device_create(hello_drv_cdev_class, NULL, dev, NULL, "hello_drv_cdev_device");

  return 0;
}

/* 4.出口函数注销file_operations结构体 */
static void hello_cdev_exit(void) {
  device_destroy(hello_drv_cdev_class, dev);
  class_destroy(hello_drv_cdev_class);

  cdev_del(&hello_drv_cdev);
  unregister_chrdev_region(dev, 1);
}

/* 5.其它信息 */
module_init(hello_cdev_init);
module_exit(hello_cdev_exit);
MODULE_LICENSE("GPL");
