#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"


#define STDIN 0
#define BUF_SZ 512

static char buf[1024];
static char *args[MAXARG];
static int arg_num_init = 0;
static int argnum = 0;

int readline(int fd) {
    char readbuf[2];
    argnum = arg_num_init;
    int offset = 0;
    while (1) {
        // 找到第一个非空字符
        while (1) {
            if (read(fd, readbuf, 1) != 1) {
                return 0;
            }
            if (readbuf[0] == '\n') {
                return 1;
            }
            if (readbuf[0] != ' ' && readbuf[0] != '\t') {
                args[argnum++] = buf + offset;
                buf[offset++] = readbuf[0];
                break;
            }
        }
        // 找到一个 arg
        while (1) {
            if (read(fd, buf + offset, 1) != 1) {
                buf[offset] = '\0';
                return 0;
            }
            if (buf[offset] == '\n') {
                buf[offset] = '\0';
                return 1;
            }
            if (buf[offset] == ' ' || buf[offset] == '\t') {
                buf[offset++] = '\0';
                break;
            }
            ++offset; 
        }
    }
}


int
main(int argc, char *argv[])
{
    int pid, status;
    // 检查命令行参数个数是否足够
    if (argc < 2) {
        fprintf(2, "Usage: xargs command ...\n");
        exit(1);
    }

    // 获取要执行的命令
    char *command = argv[1];

    // 初始化参数个数
    arg_num_init = argc - 1;

    // 复制命令行参数到 args 数组
    args[0] = command;
    for (int i = 1; i < argc; i++) {
        args[i] = argv[i + 1];
    }

    int flag = 1;
    // 循环等待用户输入
    while (flag) {
        // 读取用户输入
        flag = readline(STDIN);

        // 设置参数列表的结束符
        args[argnum] = 0;

        // 如果用户没有输入并且参数列表已经处理完毕，则退出程序
        if (flag == 0 && argnum == arg_num_init) {
            exit(0);
        }

        // 创建子进程
        pid = fork();

        // 子进程执行
        if (pid == 0) {
            // 执行命令
            exec(command, args);

            // 如果执行失败，则打印错误信息并退出
            printf("exec failed!\n");
            exit(1);
        } else {
            // 父进程等待子进程结束
            wait(&status);
        }
    }

    // 程序正常退出
    exit(0);
}
