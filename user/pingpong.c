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