
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define CMD_TRIG 100
static int fd;

/*
 * ./sr04_test /dev/sr04_device
 *
 */
int main(int argc, char **argv) {
  int val;
  struct pollfd fds[1];
  int timeout_ms = 5000;
  int ret;
  int flags;

  int i;

  /* 1. 判断参数 */
  if (argc != 2) {
    printf("Usage: %s <dev>\n", argv[0]);
    return -1;
  }

  /* 2. 打开文件 */
  fd = open(argv[1],
            O_RDWR); // O_RDWR为非阻塞方式 | O_NONBLOCK为阻塞方式
  if (fd == -1) {
    printf("can not open file %s\n", argv[1]);
    return -1;
  }

  while (1) {

    // 调用ioctl函数发出trig信号
    ioctl(fd, CMD_TRIG);
    printf("I am goning to read distance: \n");

    if (read(fd, &val, 4)) {
      printf("get distance: %d cm\n", val * 17 / 1000000);
    } else {
      printf("get distance err!\n");
    }

    sleep(1);
  }

  close(fd);

  return 0;
}
