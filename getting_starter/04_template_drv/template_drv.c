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

unsigned int major;              // 主设备号
static struct class *gpio_class; // 创建class

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
        131,
        0,
        "gpio_gpio1",
        1,
    },
    {
        132,
        0,
        "gpio_gpio2",
        0,
    },
};

/* 环形缓冲区 */
#define BUF_LEN 128
static int g_keys[BUF_LEN];
static int r, w;    // 环形缓冲区标识位：全局变量

#define NEXT_POS(x) ((x + 1) % BUF_LEN)  // 环形缓冲区标识为移动操作（取模操作，巧妙）

/* 判断环形缓冲区是否为空 */
static int is_key_buf_empty(void) {
  return (r == w);
}

/* 判断环形缓冲区是否满了 */
static int is_key_buf_full(void) { return (r == NEXT_POS(w)); }

/* 将数据写入环形缓冲区，通过全局变量w控制位置 */
static void put_key(int key) {
  if (!is_key_buf_full()) {
    g_keys[w] = key;
    w = NEXT_POS(w);
  }
}

/* 读取环形缓冲区数据，当前位置由全局变量r控制 */
static int get_key(void) {
  int key = 0;
  if (!is_key_buf_empty()) {
    key = g_keys[r];
    r = NEXT_POS(r);
  }
  return key;
}

/* 创建一个等待队列 */
static DECLARE_WAIT_QUEUE_HEAD(gpio_wait);

/* 计时器相关处理，在中间获取按键值，并通过按键值判断高低电平状态，并最终判断按键是按下还是弹出
 */
static void key_timer_expire(unsigned long data) {
  /* data ==> gpio */
  // struct gpio_desc *gpio_desc = from_timer(gpio_desc, t, key_timer);
  struct gpio_desc *gpio_desc = (struct gpio_desc *)data;
  int val; // 记录按键值
  int key; // 记录按键状态

  val = gpio_get_value(
      gpio_desc->gpio); // 获取按键值，不是1就是2（做过设置，数组初始化）

  // printk("key_timer_expire key %d %d\n", gpio_desc->gpio, val);
  key = (gpio_desc->key) | (val << 8); // 或操作判断按键值
  put_key(key); // 如果应用程序来不及读取（卡死了），放入环形缓冲区供后续操作
  wake_up_interruptible(
      &gpio_wait); // 如果有其他东西在等待按键的时候，就需要到等待队列中将其唤醒
  kill_fasync(&button_fasync, SIGIO,
              POLL_IN); // 如果是异步通知的话，需要将信号发给某个进程
}

/* 中断处理函数 */
static irqreturn_t gpio_key_isr(int irq, void *dev_id) {
  struct gpio_desc *gpio_desc = dev_id;
  // 中断函数处理，打印gpio引脚编号
  printk("gpio_key_isr key %d irq happened\n", gpio_desc->gpio);

  // 修改定时器超时时间
  mod_timer(&gpio_desc->key_timer, jiffies + HZ / 5);
  return IRQ_HANDLED;
}

/* 2.实现fops结构体中的函数 */
/*
 * 打开文件获取文件节点句柄
 */
static int gpio_open(struct inode *node, struct file *filp) {
  printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
  return 0;
}
/*
 * 读取文件filp的数据，将其写入buf空间
 */
static ssize_t gpio_read(struct file *file, char __user *buf, size_t size,
                         loff_t *offset) {

  // printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
  int err;
  int key;

  // 如果buf空间数据为空并且应用程序传入的flag是非阻塞方式，则直接返回错误
  if (is_key_buf_empty() && (file->f_flags & O_NONBLOCK))
    return -EAGAIN;

  wait_event_interruptible(gpio_wait, !is_key_buf_empty());
  key = get_key();
  err = copy_to_user(buf, &key, 4);

  return 4;
}
/*
 * 将buf空间中的数据写入文件filp
 */
static ssize_t gpio_write(struct file *file, const char __user *buf,
                          size_t size, loff_t *offset) {
  unsigned char ker_buf[2];
  int err;

  if (size != 2)
    return -EINVAL;

  err = copy_from_user(ker_buf, buf, size);

  if (ker_buf[0] >= sizeof(gpios) / sizeof(gpios[0]))
    return -EINVAL;

  gpio_set_value(gpios[ker_buf[0]].gpio, ker_buf[1]);
  return 2;
}
/*
 * 异步通知
 */
int gpio_fasync(int fd, struct file *file, int on) {
  if (fasync_helper(fd, file, on, &button_fasync) >= 0)
    return 0;
  else
    return -EIO;
}
/*
 * poll
 */
static unsigned int gpio_drv_poll(struct file *fp, poll_table *wait) {
  // printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
  poll_wait(fp, &gpio_wait, wait);
  return is_key_buf_empty() ? 0 : POLLIN | POLLRDNORM;
}

/* 1.构建file_operations结构体 */
static struct file_operations gpio_ops = {
    .owner = THIS_MODULE,
    .open = gpio_open,
    .read = gpio_read,
    .write = gpio_write,
    .fasync = gpio_fasync,
    .poll = gpio_drv_poll,
};

/* 3.入口函数注册file_operations结构体 */
static int __init gpio_init(void) {

  int i;
  int err;                                      // 注册中断返回值
  int count = sizeof(gpios) / sizeof(gpios[0]); // 获取操作引脚个数

  // 注册中断
  for (i = 0; i < count; i++) {
    gpios[i].irq = gpio_to_irq(gpios[i].gpio); // 将gpio引脚转换为对应的中断号

    // 设置计时器，用于按键消抖，同时可以获取按键值，根据按键值继续判断其它逻辑等等
    setup_timer(&gpios[i].key_timer, key_timer_expire,
                (unsigned long)&gpios[i]);
    // timer_setup(&gpios[i].key_timer, key_timer_expire, 0);
    gpios[i].key_timer.expires = ~0;
    add_timer(&gpios[i].key_timer);

    // 注册中断
    err = request_irq(gpios[i].irq, gpio_key_isr,
                      IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "gpio_key",
                      &gpios[i]);
  }

  major = register_chrdev(0, "gpio_drv_cdev", &gpio_ops);

  /* 创建class */
  gpio_class = class_create(THIS_MODULE, "gpio_class");
  if (IS_ERR(gpio_class)) {
    pr_warn("Unable to create backlight class; errno = %ld\n",
            PTR_ERR(gpio_class));
    return PTR_ERR(gpio_class);
  }

  /* 创建device */
  device_create(gpio_class, NULL, MKDEV(major, 0), NULL, "gpio_device");

  return err;
}

/* 4.出口函数注销fops结构体 */
static void __exit gpio_exit(void) {
  device_destroy(gpio_class, MKDEV(major, 0));
  class_destroy(gpio_class);
  unregister_chrdev(major, "gpio_drv_cdev");
}

/* 5.其它信息 */
module_init(gpio_init);
module_exit(gpio_exit);
MODULE_LICENSE("GPL");