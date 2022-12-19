#include "asm-generic/gpio.h"
#include "asm/uaccess.h"
#include "linux/printk.h"
#include "linux/rtnetlink.h"
#include <linux/module.h>
#include <linux/poll.h>

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/kmod.h>
#include <linux/major.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/timer.h>
#include <linux/tty.h>

unsigned int major;             // 主设备号
static struct class *led_class; // 创建class

/* 定义led描述资源 */
struct led_desc {
  int gpio;   // gpio引脚
  char *name; // led名字
  int key;    // 按键值，用于逻辑判断
};

/* 初始化led资源 */
static struct led_desc leds[2] = {
    {
        .gpio = 131,
        .name = "led1",
        .key = 1,
    },
    {
        .gpio = 132,
        .name = "led2",
        .key = 2,
    },

};

/* 2.应用程序需要或取led灯的亮灭状态 */
static int led_read(struct file *file, char __user *buf, size_t size,
                    loff_t *offset) {
  printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

  // 用户buf传入一个字符数组且长度为2；buf[0]=index,buf[1]=status
  int tmp_buf[2]; // 不能直接赋值，应用层到用户层的数据交互需要借助内核函数
  int err;
  int count = sizeof(leds) / sizeof(leds[0]); // 获取操作led个数

  err = copy_from_user(tmp_buf, buf, size);

  if (tmp_buf[0] >= count || size != 2) {
    return -EINVAL;
  }

  // 读取引脚值将其写入tmp_buf[1]
  tmp_buf[1] = gpio_get_value(leds[tmp_buf[0]].gpio);

  // 将tmp_buf[1]中存放的灯状态数据传递至用户空间
  // buf[0]=哪一盏灯 buf[1]=0（灯亮） buf[1]=1（灯灭）
  copy_to_user(buf, tmp_buf, 2);

  return err;
}
/* 2.应用程序控制led的亮灭 */
static int led_write(struct file *file, const char __user *buf, size_t size,
                     loff_t *offset) {
  printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
  int tmp_buf[2]; // 不能直接赋值，应用层到用户层的数据交互需要借助内核函数
  int err;
  int count = sizeof(leds) / sizeof(leds[0]); // 获取操作led个数

  err = copy_from_user(tmp_buf, buf, size);

  if (tmp_buf[0] >= count || size != 2) {
    return -EINVAL;
  }

  // 读取引脚值将其写入tmp_buf[1]
  gpio_set_value(leds[tmp_buf[0]].gpio, tmp_buf[1]);
}

/* 1.构建fops结构体 */
static struct file_operations led_ops = {
    .owner = THIS_MODULE,
    .read = led_read,
    .write = led_write,
};

/* 3.入口函数注册fops结构体 */
static int __init led_init(void) {

  int index;                                 // 描述led索引
  int nums = sizeof(leds) / sizeof(leds[0]); // led灯数量
  int ret;                                   // 注册中断返回值

  // 3.1 设置gpio引脚
  for (index = 0; index < nums; index++) {
    // 3.1.1 请求gpio
    ret = gpio_request(
        leds[index].gpio,
        leds[index].name); // 使用gpio_request请求gpio，有可能被其它功能占用了
    if (ret < 0) {
      printk("can not request gpio %d. led:%s\n", leds[index].gpio,
             leds[index].name);
      return -ENODEV;
    }

    // 3.1.2 set pin as output
    gpio_direction_output(leds[index].gpio, 1);
  }

  major = register_chrdev(0, "led_drv_cdev", &led_ops);
  // 3.2 创建class
  led_class = class_create(THIS_MODULE, "led_class");
  if (IS_ERR(led_class)) {
    pr_warn("Unable to create backlight class; errno = %ld\n",
            PTR_ERR(led_class));
    return PTR_ERR(led_class);
  }

  // 3.3 创建device
  device_create(led_class, NULL, MKDEV(major, 0), NULL, "led_dev");

  return ret;
}

/* 4.出口函数卸载 */
static void __exit led_exit(void) {

  int index;
  int count = sizeof(leds) / sizeof(leds[0]); // 获取操作引脚个数

  device_destroy(led_class, MKDEV(major, 0));
  class_destroy(led_class);
  unregister_chrdev(major, "led_drv_cdev");

  for (index = 0; index < count; index++) {
    gpio_free(leds[index].gpio);
  }
}

/* 5.其它信息 */
module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");