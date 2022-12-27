
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int fd;

/*
 * ./button_test /dev/100ask_button0
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
  fd =
      open(argv[1], O_RDWR | O_NONBLOCK); // O_RDWR为非阻塞 | O_NONBLOCK为阻塞方
  if (fd == -1) {
    printf("can not open file %s\n", argv[1]);
    return -1;
  }

  for (i = 0; i < 10; i++) {
    if (read(fd, &val, 4) == 4)
      printf("get button: 0x%x\n", val);
    else
      printf("get button: -1\n");
  }

  flags = fcntl(fd, F_GETFL);
  fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);

  while (1) {
    if (read(fd, &val, 4) == 4)
      printf("get button: 0x%x\n", val);
    else
      printf("while get button: -1\n");
  }

  close(fd);

  return 0;
}
