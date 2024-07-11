# Lazyallocation

## 实验相关

这个任务的主要目的是实现内存的懒分配（Lazy page allocation）。作系统可以使用页表硬件的技巧之一是延迟分配用户空间堆内存（lazy allocation of user-space heap memory）。

Xv6应用程序使用`sbrk()`系统调用向内核请求堆内存。在我们给出的内核中，`sbrk()`分配物理内存并将其映射到进程的虚拟地址空间。内核为一个大的内存分配请求进行分配和映射内存可能需要很长时间。单独分配一个应用程序的内存开销或许很低，但操作系统上往往同时运行多个应用程序，分配数量可能也比较大。此外，有些**应用程序申请分配的内存比它实际使用的大的多**，或许是为了将来使用而申请，但这些过早开辟的内存将在一段时间内空闲导致内存空间的浪费。因此，懒分配应运而生。

为了让`sbrk()`在这些情况下更快地完成，复杂的内核会延迟分配用户内存。也就是说，`sbrk()`不分配物理内存，只是记住分配了哪些用户地址，并在用户页表中将这些地址标记为无效。当进程第一次尝试使用延迟分配中给定的页面时，CPU生成一个页面错误（page fault），内核通过分配物理内存、置零并添加映射来处理该错误。

## 任务一、Eliminate allocation from sbrk()

**xv6操作系统中，sys\_sbrk() 函数是程序申请开辟内存的一个系统调用。它在申请开辟内存时，只要剩余空间大小大于申请内存的大小就进行分配。现在需要将其改成，申请内存时仅在虚拟地址空间进行分配而不分配真实的物理内存。考虑到内存申请大小 n 有可能为负数，增加相应的判断和处理逻辑，当 n < 0 时，释放 n 大小的内存空间。**

以下是对 sys_sbrk() 的修改：

```
uint64
sys_sbrk(void)
{
  int addr;
  int n;
  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;//这里获取虚拟地址
  /* 以前的，为进程开辟大小为 n 的真实内存空间
  if(growproc(n) < 0)
    return -1;
  */
  //下面这行我们添加的
  if (addr + n >= MAXVA || addr + n <= 0)//防止越界
    return addr;
  myproc()->sz += n;//仅仅将虚拟地址增加 n 
  if(n < 0)//如果是缩小内存
    uvmdealloc(p->pagetable, addr, p->sz);//释放大小为 n 的内存空间
  return addr;
}
```

## 任务二、Lazy allocation

**1.实现 xv6 操作系统的懒分配。在上一个任务中，实现了为程序分配“假”内存。如果程序访问到这些假内存会发生什么呢？答案是会触发中断处理程序 **`usertrap()`。在前一个实验中知道了`usertrap()`根据中断码判断中断原因然后跳入对应的中断处理程序，这里的中断原因是缺少内存映射。查询 xv6 系统手册知道 page fault 的中断码是 13 和 15，因此在 `usertrap()`中对 `r_scause()` 中断原因进行判断，如果是13 或 15 ，则说明没有找到虚拟地址对应的物理地址，此时虚拟地址存储在 **STVAL** 寄存器中，取出该地址进行分配。如果**申请物理地址没成功或者虚拟地址超出范围了，那么杀掉进程**，完整的 `usertrap()` 代码如下：

```
void
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  
  // save user program counter.
  p->trapframe->epc = r_sepc();
  
  if(r_scause() == 8){
    // system call

    if(p->killed)
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sstatus &c registers,
    // so don't enable until done with those registers.
    intr_on();

    syscall();
  } else if((which_dev = devintr()) != 0){
    // ok
  } else if(r_scause() == 13 || r_scause() == 15) {//判断中断原因是 13 或 15时，进行对应处理
    uint64 va = r_stval();//获得虚拟地址
    uint64 pa = (uint64)kalloc();//开辟物理内存
    if (pa == 0) {//分配失败
      p->killed = 1;//杀死进程
    } else if (va >= p->sz || va <= PGROUNDDOWN(p->trapframe->sp)) {//防止地址越界
      kfree((void*)pa);
      p->killed = 1;
    } else {
      va = PGROUNDDOWN(va);
      memset((void*)pa, 0, PGSIZE);
      if (mappages(p->pagetable, va, PGSIZE, pa, PTE_W | PTE_U | PTE_R) != 0) {
        kfree((void*)pa);
        p->killed = 1;
      }
    }
    // lazyalloc(va);
  } else {
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    p->killed = 1;
  }

  if(p->killed)
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2)
    yield();

  usertrapret();
}
```

**父进程创建子进程时还需要用到**`fork()`函数，因此还要处理**kernel/proc.c**的`fork()`函数中父进程向子进程拷贝时的Lazy allocation 情况：

```
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  np->parent = p;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  np->state = RUNNABLE;

  release(&np->lock);

  return pid;
}
```

**可以看出**`fork()`是通过`uvmcopy()`将父进程页表向子进程拷贝的。对于`uvmcopy()`的处理和 `uvmunmap()`一致，只需要将PTE不存在和无效的两种情况由引发panic改为continue跳过即可，以下是对`uvmcopy()`函数的修改：

```
if((pte = walk(old, i, 0)) == 0)
  //panic("uvmcopy: pte should exist");  //注释掉原来这行
  continue; //添加
if((*pte & PTE_V) == 0)
  //panic("uvmcopy: page not present");  //注释掉原来这行
  continue; //添加
```

**2.以上修改了内存分配函数，使得进程申请内存时申请到的是仅仅是虚拟内存而非真实物理内存。倘若进程在整个生命周期都没有使用申请到的虚拟内存就结束了，调用原本的进程内存释放函数时就会出现错误。因此，这里对内存释放函数进行相应的处理：**

```
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0)//发现va没有对应的页表项（只有虚拟地址没有对应的物理页）直接跳过
      //panic("uvmunmap: walk");
      continue;
    if((*pte & PTE_V) == 0)//发现va对应的页表项是无效的（没有映射到对应的物理内存）直接跳过
      continue;
      //panic("uvmunmap: not mapped");
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    *pte = 0;
  }
}
```

## 实现结果
