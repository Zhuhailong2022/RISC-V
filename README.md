# 任务一（sleep）

这是一个简单的任务，在用户态直接调用提供的 sleep 方法即可。包含哪些头文件可以参考其他的 user 目录下的 .c 文件。

实现步骤：

1. 在user目录下添加`sleep.c`文件
2. 在`Makefile`文件的`UPROGS`字段中 添加对应代码。

/sleep.c

```c
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
```

编译和链接用户程序：Makefile 会根据 UPROGS 字段中指定的程序来编译和链接相应的用户程序。
如果你没有在 UPROGS 字段中添加你的程序，那么你的程序就不会被编译，也不会被包含在最终的文件系统镜像中。
所以在写完 .c 代码后，需要在 Makefile 文件中的 UPROGS 字段中添加 $U/_[xxx]\，然后才能使用 ./grade-lab-util 进行测试。

实验结果：

![image](https://github.com/Zhuhailong2022/RISC-V/assets/115600879/37e09ed5-70fb-4d39-97a1-cfe6610db48c)


# 任务二（pingpong）

使用 pipe() 和 fork() 实现父进程发送一个字符，子进程成功接收该字符后打印 received ping，再向父进程发送一个字符，父进程成功接收后打印 received pong。

1. 文件描述符 FD
   这里需要懂得什么是文件描述符。在 Linux 操作系统中，一切皆文件，内核通过文件描述符访问文件。每个进程都会默认打开3个文件描述符,即0、1、2。其中0代表标准输入流（stdin）、1代表标准输出流（stdout）、2代表标准错误流（stderr）。可以使用 > 或 >> 的方式重定向。
2. 管道 PIP
   管道是操作系统进程间通信的一种方式，可以使用 pipe() 函数进行创建。有在管道创建后，就有两个文件描述符，一个是负责读，一个是负责写。两个描述符对应内核中的同一块内存，管道的主要特点：数据在管道中以字节流的形式传输，由于管道是半双工的，管道中的数据只能单向流动。匿名管道只能用于具有亲缘关系的进程通信，父子进程或者兄弟进程。在 fork() 后，父进程和子进程拥有相同的管道描述符，通常为了安全起见，关闭一个进程的读描述符和另一个进程的写描述符，这样就可以让数据更安全地传输。在本实验中，我使用一个管道进行父子进程间的双向通信，实验中最重要的一点是保证同一时刻只有一个进程在使用管道。

实现步骤：

1. 在user目录下添加pingpong.c
2. 在Makefile的 UPROGS 字段添加相应字段。

/pingpong.c

```c
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
```

实验结果：

![image](https://github.com/Zhuhailong2022/RISC-V/assets/115600879/2290694d-d2c9-4700-8c38-20bcc6758c9d)


# 任务三（primes）

这里需要使用 fork 子进程和进程间通信的技术实现 2\~35 以内质数的筛选。

![img](https://xv6.dgs.zone/labs/requirements/images/p1.png)

实现的大致思路是：

* 第一个进程将 2\~35 写入管道
* 然后 fork 第二个进程，逐个从管道中读出数字，再将符合条件（第一个进程去掉能被2整除的）的数字送入下一个管道，给第三个进程（去掉能被3整除的）
* 第三个进程类似…
* 父进程等待子进程结束，最后退出进程

实现步骤：

1. 在user目录下添加primes.c
2. 在Makefile的 UPROGS 字段添加相应字段。

```cpp
#include "kernel/types.h"
#include "user/user.h"

#define RD 0
#define WR 1

const uint INT_LEN = sizeof(int);

// 读取管道的第一个数据，并将它打印出来
int lpipe_first_data(int lpipe[2], int *dst)
{
  if (read(lpipe[RD], dst, sizeof(int)) == sizeof(int)) {
    printf("prime %d\n", *dst);
    return 0;
  }
  return -1;
}

// 将无法整除的数据传递入右管道
void transmit_data(int lpipe[2], int rpipe[2], int first)
{
  int data;
  // 从左管道读取数据
  while (read(lpipe[RD], &data, sizeof(int)) == sizeof(int)) {
    // 将无法整除的数据传递入右管道
    if (data % first)
      write(rpipe[WR], &data, sizeof(int));
  }
  close(lpipe[RD]);
  close(rpipe[WR]);
}
// 递归的寻找质数
void primes(int lpipe[2])
{
  close(lpipe[WR]);
  int first;
  if (lpipe_first_data(lpipe, &first) == 0) {
    int p[2];
    pipe(p); // 当前的管道
    transmit_data(lpipe, p, first);

    if (fork() == 0) {
      primes(p);    // 递归的思想，但这将在一个新的进程中调用
    } else {
      close(p[RD]);
      wait(0);
    }
  }
  exit(0);
}

int main(int argc, char const *argv[])
{
  int p[2];
  pipe(p);

  for (int i = 2; i <= 35; ++i) //父进程从管道写入端写入初始数据
    write(p[WR], &i, INT_LEN);

  if (fork() == 0) {//子进程调用primes函数
    primes(p);
  } else {
    close(p[WR]);
    close(p[RD]);
    wait(0);
  }
  exit(0);
}
```
实验结果：

![image](https://github.com/Zhuhailong2022/RISC-V/assets/115600879/11e22721-d23f-438a-b244-df1671d8657c)


# 任务四（find）

find 命令实现在路径中查找特定文件名的文件。

在代码中需要对子目录进行递归查询，这里对文件和目录的遍历可以参考 `ls.c`文件中的代码。了解描述文件信息和目录的结构体：

```c
// 文件信息结构体
// 其中type表明了文件的类型是：文件、目录还是设备
#define T_DIR     1   // Directory
#define T_FILE    2   // File
#define T_DEVICE  3   // Device

struct stat {
  int dev;     // File system's disk device
  uint ino;    // Inode number
  short type;  // Type of file
  short nlink; // Number of links to file
  uint64 size; // Size of file in bytes
};

// 所谓目录，就是一系列dirent结构组成的顺序序列
#define DIRSIZ 14

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};
```

实现思路：

1. 对比当前路径下的文件是否为指定的文件
2. 如果有文件夹要继续进行递归
3. 不要递归`.`和`..`

实现步骤：

1. 在user目录下添加find.c
2. 在Makefile的 UPROGS 字段添加相应字段。

//find.c

```cpp
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"


/**
 * 获取一个 path 的名称，比如 `./dira/b` 将返回 `b`
*/
char* fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // 找到倒数第一个 '/'
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // 如果 p 指向的字符串长度大于等于 DIRSIZ，则直接返回 p 指针
  // 如果不大于等于DIRSIZ则复制到 buf
  if(strlen(p) >= DIRSIZ)
    return p;
  // 将 p 指向的字符串复制到 buf 中
  memmove(buf, p, strlen(p));
  // 在 buf 的末尾添加字符串结束符
  buf[strlen(p)] = '\0';
  // 返回 buf 指针
  return buf;
}

void find(char *path, char const * const target) {
    int fd;
    struct dirent de;
    struct stat st;

    // 打开目录
    if ((fd = open(path, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }
    // 获取文件状态
    if (fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        close(fd);
        return;
    }

    // 根据文件类型进行处理
    switch (st.type) {
        // 如果是文件
    case T_FILE: {
        // 格式化文件名
        char *fname = fmtname(path);
        // 如果文件名与目标名相同，则打印路径
        if (strcmp(fname, target) == 0) {
            printf("%s\n", path);
        }
        break;
    }
        // 如果是目录
    case T_DIR: {
        // 缓冲区
        char buf[512], *p;
        // 检查路径长度是否过长
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
            printf("find: path too long\n");
            break;
        }
        // 复制路径到缓冲区
        strcpy(buf, path);
        // 指向路径的末尾
        p = buf + strlen(buf);
        // 添加目录分隔符
        *p++ = '/';
        // 读取目录项
        while (read(fd, &de, sizeof(de)) == sizeof(de)) {
            // 跳过无效项
            if (de.inum == 0)
                continue;
            // 将目录项名称复制到缓冲区
            memmove(p, de.name, DIRSIZ);
            // 添加字符串结束符
            p[DIRSIZ] = 0;
            // 获取目录项的状态
            if (stat(buf, &st) < 0) {
                printf("find: cannot stat %s\n", buf);
                continue;
            }
            // 格式化目录项名称
            char* dir_name = fmtname(buf);
            // 跳过当前目录和上级目录
            if (strcmp(dir_name, ".") != 0 && strcmp(dir_name, "..") != 0) {
                // 递归查找
                find(buf, target);
            }
        }
        break;
    }
    }
    // 关闭文件描述符
    close(fd);
}

int
main(int argc, char *argv[])
{
    // 如果参数个数小于2
    if (argc < 2) {
        // 打印使用说明
        fprintf(2, "Usage: find path file\n");
        // 退出程序，返回错误码1
        exit(1);
    }

    // 路径参数
    char *path = argv[1];
    // 目标文件名参数
    char const *target = argv[2];

    // 调用find函数，传入路径和目标文件名
    find(path, target);

    // 退出程序，返回成功码0
    exit(0);
}

```

实验结果：

![image](https://github.com/Zhuhailong2022/RISC-V/assets/115600879/b33ce1fd-286c-43a8-ae18-db4911b3ff31)


# 任务五（xargs）

xargs(extensive arguments)是Linux系统中的一个很重要的命令，它一般==通过管道来和其他命令一起调用==，来将额外的参数传递给命令，这个小问题就是实现自己版本的xargs命令。

```bash
$ echo hello too | xargs echo bye
  bye hello too
$
```

把 | 前面命令的 stdout 输出当作 xargs 命令的 stdin输入，然后 xargs 依次读取 stdin 的每一行，把每一行拼到 xargs 后面“echo byte”字符串后面并当成一个命令来执行。所以上面这个命令执行的结果等价于 xargs echo bye hello too。

```c
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


```

实验结果：

![image](https://github.com/Zhuhailong2022/RISC-V/assets/115600879/fbf63878-ac6f-498b-92fb-8cea363599a2)

