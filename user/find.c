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
            // 对于每个目录项（除了 "." 和 ".."），递归调用 find 函数。
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
