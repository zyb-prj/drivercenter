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

/* 构建环形缓冲区资源 */
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

/* 定义led描述资源 */
struct led_desc {
  int gpio;                    // gpio引脚
  int irq;                     // 中断号
  char *name;                  // led名字
  int key;                     // 按键值，用于逻辑判断
  struct timer_list led_timer; // 定时器
};

/* 初始化led资源 */
static struct led_desc leds[2] = {
    {
        .gpio = 131,
        .irq = 0,
        .name = "red",
        .key = 1,
    },
    {
        .gpio = 132,
        .irq = 1,
        .name = "green",
        .key = 2,
    },

};

/*
 * 定时器超时函数，调用此函数说明数据稳定，抖动已被消除：
 *  1：参数解析并转换
 *  2：或取按键值并作逻辑判断
 *  3：如果应用程序来不及读取（卡死了），放入环形缓冲区供后续操作
 *  4：如果有其他东西在等待按键的时候，就需要到等待队列中将其唤醒
 *  5：如果是异步通知的话，需要将信号发给某个进程
 */
static void led_timer_expire(unsigned long data) {
  struct led_desc *led_desc = (struct led_desc *)data; // data ==> gpio

  int val; // 记录按键值
  int key; // 记录按键标识

  val = gpio_get_value(led_desc->gpio); // 内核函数读取gpio值，01 or 10
  key = (led_desc->key) | (val << 8);
  put_key(key);
}

/* 中断处理函数 */
static irqreturn_t led_irq_handler(int irq, void *dev_id) {
  struct gpio_desc *gpio_desc = dev_id;
  // 中断函数处理，打印gpio引脚编号
  printk("led_irq_handler key %d irq happened\n", gpio_desc->gpio);

  // 修改定时器超时时间
  mod_timer(&gpio_desc->led_timer, jiffies + HZ / 5);
  return IRQ_HANDLED;
}

/* 2.实现fops结构体函数 */
static int led_open(struct inode *node, struct file *file) {
  printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
}
static int led_read(struct file *file, char __user *buf, size_t size,
                    loff_t *offset) {
  printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
}
static int led_write(struct file *file, const char __user *buf, size_t size,
                     loff_t *offset) {
  printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
}
static int led_close(struct inode *node, struct file *file) {
  printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
}
static int led_fasync(int fd, struct file *file, int on) {
  printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
}
static unsigned int led_poll(struct file *file, poll_table *wait) {
  printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
}

/* 1.构建fops结构体 */
static struct file_operations led_ops = {
    .owner = THIS_MODULE,
    .open = led_open,
    .read = led_read,
    .write = led_write,
    .release = led_close,
    .fasync = led_fasync,
    .poll = led_poll,
};

/* 3.入口函数注册fops结构体 */
static int __init led_init(void) {

  int index;                                 // 描述led索引
  int nums = sizeof(leds) / sizeof(leds[0]); // led灯数量
  int err;                                   // 注册中断返回值

  for (index = 0; index < nums; index++) {
    // 设置中断号
    leds[index].irq =
        gpio_to_irq(leds[index].gpio); // 将引脚编号转换为对应的中断号

    // 设置计时器，计时器超时函数中获取数据并做对应的逻辑处理
    setup_timer(&leds[index].led_timer, led_timer_expire,
                (unsigned long)&leds[index]);
    leds[index].led_timer.expires = ~0; // 定时器超时时间初始化为无穷大
    add_timer(&leds[index].led_timer);

    // 注册中断
    err = register_irq(leds[index].irq, led_irq_handler,
                       IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "led_irq",
                       &leds[index]);
  }
}

/* 4.出口函数卸载 */
static void __exit led_exit(void) {}

/* 5.其它信息 */
module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");