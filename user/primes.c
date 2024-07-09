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
