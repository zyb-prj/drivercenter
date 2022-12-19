
#include <elf.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int fd;

/*
 * ./led_test  <0|1|2> on        // 点灯
 * ./led_test  <0|1|2> off       // 熄灯
 * ./led_test  <0|1|2>           // 读取灯的状态
 */
int main(int argc, char **argv) {
  int ret;
  int usr_buf[2];

  /* 1. 判断参数 */
  if (argc < 2) {
    printf("Usage: %s <0|1|2> [on|off]\n", argv[0]);
    return -1;
  }

  /* 2. 打开文件（设备节点） */
  fd = open("/dev/led_dev", O_RDWR);
  if (fd == -1) {
    printf("can not open file /dev/led_dev.\n");
    return -1;
  }

  // 写数据，点灯
  if (argc == 3) {
    usr_buf[0] = strtol(argv[1], NULL, 0);

    if (strcmp(argv[2], "on") == 0) {
      usr_buf[1] = 0;
    } else {
      usr_buf[1] = 1;
    }
    ret = write(fd, usr_buf, 2);
  } else {
    usr_buf[0] = strtol(argv[1], NULL, 0);
    ret = read(fd, usr_buf, 2);

    if (ret == 2) {
      printf("%s is %s!\n", argv[1], usr_buf[1] == 0 ? "on" : "off");
    }
  }

  close(fd);

  return 0;
}
