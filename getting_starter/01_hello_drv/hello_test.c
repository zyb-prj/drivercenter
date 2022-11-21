#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

/*
*  驱动程序提供打开文件并读写数据的功能
*  终端操作应用程序并提供相应参数做测试
*  操作格式如下：
*       写： ./hello_test /dev/xxx zyb
*       读： ./hello_test /dev/xxx
*
*  argc表示参数个数
*/
int main(int argc, char **argv) {
    
    // 定义节点句柄
    int fd;
    // 定义read/write函数返回的值大小
    int len;
    // 定义读数据buf空间
    char buf[100];

    // 入参校验
    if (argc < 2) {
        printf("Usage: %s <dev> [string] \n", argv[0]);
        return -1;
    }

    // open，打开第0个文件，可读可写
    fd = open(argv[0], O_RDWR);
    if (fd < 0)
    {
        printf("can not open file: %s\n", argv[0]);
        return -1;
    }

    // write
    if (argc == 3)
    {
        len = write(fd, argv[2], strlen(argv[2])+1);
        printf("write ret = %d\n", len);
    }

    // read
    if (argc == 2) {
        // 读100字节，从fd-->buf
        len = read(fd, buf, 100);

        // 上面指定要读取100字节，防止读到99字节后输出乱码
        buf[99] = '\0';
        printf("write str : %s\n", buf);
    }

    // close
    close(fd);

    return 0;
}