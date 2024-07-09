任务一（sleep）
这是一个简单的任务，在用户态直接调用提供的 sleep 方法即可，纯属为了好上手。包含哪些头文件可以参考其他的 user 目录下的 .c 文件。

/sleep.c

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    fprintf(2, "Usage: sleep time...\n");
    exit(1);
  }
  int time = atoi(argv[1]);//获得命令行中的第二个参数，睡眠时长
  sleep(time);// 系统调用的 sleep，声明在 `user/user.h` 中
  exit(0);
}
编译和链接用户程序：Makefile 会根据 UPROGS 字段中指定的程序来编译和链接相应的用户程序。
如果你没有在 UPROGS 字段中添加你的程序，那么你的程序就不会被编译，也不会被包含在最终的文件系统镜像中。
所以在写完 .c 代码后，需要在 Makefile 文件中的 UPROGS 字段中添加 $U/_[xxx]\，然后才能使用 ./grade-lab-util 进行测试。


任务二（pingpong）
使用 pipe() 和 fork() 实现父进程发送一个字符，子进程成功接收该字符后打印 received ping，再向父进程发送一个字符，父进程成功接收后打印 received pong。

文件描述符 FD
这里需要懂得什么是文件描述符。在 Linux 操作系统中，一切皆文件，内核通过文件描述符访问文件。每个进程都会默认打开3个文件描述符,即0、1、2。其中0代表标准输入流（stdin）、1代表标准输出流（stdout）、2代表标准错误流（stderr）。可以使用 > 或 >> 的方式重定向。

管道 PIPE
管道是操作系统进程间通信的一种方式，可以使用 pipe() 函数进行创建。有在管道创建后，就有两个文件描述符，一个是负责读，一个是负责写。两个描述符对应内核中的同一块内存，管道的主要特点：

数据在管道中以字节流的形式传输，由于管道是半双工的，管道中的数据只能单向流动。
匿名管道只能用于具有亲缘关系的进程通信，父子进程或者兄弟进程。
在 fork() 后，父进程和子进程拥有相同的管道描述符，通常为了安全起见，关闭一个进程的读描述符和另一个进程的写描述符，这样就可以让数据更安全地传输。在本实验中，我使用一个管道进行父子进程间的双向通信，实验中最重要的一点是保证同一时刻只有一个进程在使用管道。在user目录下添加pingpong.c,然后在Makefile的 UPROGS 字段添加相应字段，代码实现如下：

/pingpong.c

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int
main(int argc, char *argv[])
{
    int fds[2], pid, n, xstatus;
    enum { BYTE_SZ = 1 };
    if (pipe(fds) != 0) {
        printf("pipe() failed\n");
        exit(1);
    }
    pid = fork();
    if (pid > 0) {  // 父进程执行的操作
        // 向子进程发送一个字节
        char parent_buf[BYTE_SZ];
        if (write(fds[1], "x", BYTE_SZ) != BYTE_SZ) {
            printf("pingpong oops 1\n");
            exit(1);
        }
        // 等待子进程结束
        wait(&xstatus);//xstatus是一个传出参数，用于wait()函数执行后返回参数的保存，wait()函数在其任意子进程结束时返回该子进程的PID，并将退出状态码存入xstatus。
        if (xstatus != 0) {//这里确保父进程与子进程不会在同一时刻使用读端或者写端
            exit(xstatus);
        }
        // 读取子进程发送过来的字节
        while ((n = read(fds[0], parent_buf, BYTE_SZ)) > 0) {
            if (parent_buf[0] != 'x') {
                printf("pingpong oops 2\n");
                exit(1);
            }
            printf("%d: received pong\n", getpid());
            close(fds[0]);
            close(fds[1]);
            exit(0);
        }
    } else if (pid == 0) {  // 子进程执行的操作
        // 等待读取父进程发送的字节
        char child_buf[BYTE_SZ];
        while ((n = read(fds[0], child_buf, BYTE_SZ)) > 0) {//read() 函数的行为：在另一方未关闭写的 fd 时，自己读取且没有更多消息时会阻塞住；
                                                            //而在另一方关闭了写的 fd 且自己没有更多消息可以读时，会立刻返回 0.
            if (child_buf[0] != 'x') {
                printf("pingpong oops 2\n");
                exit(1);
            }
            printf("%d: received ping\n", getpid());
            // 向父进程发送一个字节
            if (write(fds[1], "x", BYTE_SZ) != BYTE_SZ) {
                printf("pingpong oops 2\n");
                exit(1);
            }
            close(fds[0]);
            close(fds[1]);
            exit(0);
        }
    } else {
        printf("fork() failed\n");
        exit(1);
    }
    exit(0);
}

