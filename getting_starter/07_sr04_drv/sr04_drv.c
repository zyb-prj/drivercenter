#include "asm-generic/gpio.h"
#include "asm/gpio.h"
#include "linux/irqreturn.h"
#include "linux/timekeeping.h"
#include <linux/module.h>
#include <linux/poll.h>

#include <linux/delay.h>
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

#define CMD_TRIG 100

unsigned int major;              // 主设备号
static struct class *sr04_class; // 创建class

struct fasync_struct *button_fasync; // 异步通知

/* 自定义结构体：用于描述引脚 */
struct gpio_desc {
  int gpio;                    // 哪个引脚
  int irq;                     // 中断号
  char *name;                  // 名字
  int key;                     // 按键值（泛指）
  struct timer_list key_timer; // 定时器
};

/* 定义资源，后面会有各种设置，不用初始化完全 */
static struct gpio_desc gpios[2] = {
    {
        115,
        0,
        "gpio_sr04_trig",
    },
    {
        116,
        0,
        "gpio_sr04_echo",
    },
};

/* 环形缓冲区 */
#define BUF_LEN 128
static int g_keys[BUF_LEN];
static int r, w; // 环形缓冲区标识位：全局变量

#define NEXT_POS(x)                                                            \
  ((x + 1) % BUF_LEN) // 环形缓冲区标识为移动操作（取模操作，巧妙）

/* 判断环形缓冲区是否为空 */
static int is_key_buf_empty(void) { return (r == w); }

/* 判断环形缓冲区是否满了 */
static int is_key_buf_full(void) { return (r == NEXT_POS(w)); }

/* 将数据写入环形缓冲区，通过全局变量w控制位置 */
static void put_value(int key) {
  if (!is_key_buf_full()) {
    g_keys[w] = key;
    w = NEXT_POS(w);
  }
}

/* 读取环形缓冲区数据，当前位置由全局变量r控制 */
static int get_value(void) {
  int key = 0;
  if (!is_key_buf_empty()) {
    key = g_keys[r];
    r = NEXT_POS(r);
  }
  return key;
}

/* 创建一个等待队列 */
static DECLARE_WAIT_QUEUE_HEAD(gpio_wait);

/* 中断处理函数 */
static irqreturn_t gpio_sr04_isr(int irq, void *dev_id) {
  struct gpio_desc *gpio_desc = (struct gpio_desc *)dev_id;
  int val; // 记录按键值
  u64 start_time = 0;
  u64 end_time = 0;
  static u64 time = 0;

  val = gpio_get_value(gpio_desc->gpio);

  if (val) {
    // 上升沿记录起始时间
    start_time = ktime_get_ns();

  } else {
    // 下降沿记录结束时间，并计算时间差和距离
    end_time = ktime_get_ns();
  }

  time = start_time - end_time;

  // 将距离写入环境缓冲区
  put_value(time);

  // 如果有人在等待这个数据就唤醒它
  wake_up_interruptible(
      &gpio_wait); // 如果有其他东西在等待按键的时候，就需要到等待队列中将其唤醒

  // 如过有人需要等待信号做处理那就发信号
  kill_fasync(&button_fasync, SIGIO,
              POLL_IN); // 如果是异步通知的话，需要将信号发给某个进程

  return IRQ_HANDLED;
}

/* 2.实现fops结构体中的函数 */
/*
 * 读取文件filp的数据，将其写入buf空间
 */
static ssize_t sr04_read(struct file *file, char __user *buf, size_t size,
                         loff_t *offset) {

  // printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
  int err;
  int key;

  // 如果buf空间数据为空并且应用程序传入的flag是非阻塞方式，则直接返回错误
  if (is_key_buf_empty() && (file->f_flags & O_NONBLOCK))
    return -EAGAIN;

  wait_event_interruptible(gpio_wait, !is_key_buf_empty());
  key = get_value();
  err = copy_to_user(buf, &key, 4);

  return err;
}
/*
 * 异步通知
 */
static int sr04_fasync(int fd, struct file *file, int on) {
  if (fasync_helper(fd, file, on, &button_fasync) >= 0)
    return 0;
  else
    return -EIO;
}
/*
 * poll
 */
static unsigned int sr04_poll(struct file *fp, poll_table *wait) {
  // printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
  poll_wait(fp, &gpio_wait, wait);
  return is_key_buf_empty() ? 0 : POLLIN | POLLRDNORM;
}
/*
 * ioctl 应用程序发出trig信号
 *  command可以自定义
 */
long sr04_ioctl(struct file *file, unsigned int command, unsigned long arg) {
  // send trig
  switch (command) {
  case CMD_TRIG: {
    gpio_set_value(gpios[0].gpio, 1);
    udelay(20);
    gpio_set_value(gpios[0].gpio, 0);
  }
  }
  return 0;
}

/* 1.构建file_operations结构体 */
static struct file_operations sr04_ops = {
    .owner = THIS_MODULE,
    .read = sr04_read,
    .fasync = sr04_fasync,
    .poll = sr04_poll,
    .unlocked_ioctl = sr04_ioctl,
};

/* 3.入口函数注册file_operations结构体 */
static int __init sr04_init(void) {

  int i;
  int err;
  int count = sizeof(gpios) / sizeof(gpios[0]); // 获取操作引脚个数

  // 请求GPIO资源
  for (i = 0; i < count; i++) {
    gpio_request(gpios[i].gpio, gpios[i].name);
  }

  // 初始化trig引脚，配置为输出并且平时状态为低电平
  gpio_direction_output(gpios[0].gpio, 0);

  // 初始化echo引脚，接收超声波，注册中断，接收中断
  gpios[1].irq = gpio_to_irq(gpios[1].gpio); // 将gpio引脚转换为对应的中断号
  err = request_irq(gpios[1].irq, gpio_sr04_isr,
                    IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "gpio_sr04_isr",
                    &gpios[1]);

  major = register_chrdev(0, "sr04_drv_cdev", &sr04_ops);
  /* 创建class */
  sr04_class = class_create(THIS_MODULE, "sr04_class");
  if (IS_ERR(sr04_class)) {
    pr_warn("Unable to create backlight class; errno = %ld\n",
            PTR_ERR(sr04_class));
    return PTR_ERR(sr04_class);
  }

  /* 创建device */
  device_create(sr04_class, NULL, MKDEV(major, 0), NULL, "sr04_device");

  return err;
}

/* 4.出口函数注销fops结构体 */
static void __exit sr04_exit(void) {

  int i;
  int count = sizeof(gpios) / sizeof(gpios[0]); // 获取操作引脚个数

  device_destroy(sr04_class, MKDEV(major, 0));
  class_destroy(sr04_class);
  unregister_chrdev(major, "sr04_drv_cdev");

  for (i = 0; i < count; i++) {
    gpio_free(gpios[i].gpio);
  }

  // 释放中断
  free_irq(gpios[1].gpio, &gpios[1]);
}

/* 5.其它信息 */
module_init(sr04_init);
module_exit(sr04_exit);
MODULE_LICENSE("GPL");