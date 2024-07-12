// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

//内存引用计数结构
struct pagerefcnt {
  struct spinlock lock;
  uint8 refcount[PHYSTOP / PGSIZE];
} ref;

//增加页表 va 的引用计数
void incref(uint64 va) {
  acquire(&ref.lock);
  if(va < 0 || va > PHYSTOP) panic("wrong virtual address");
  ref.refcount[va / PGSIZE]++;
  release(&ref.lock);
}


void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&ref.lock, "ref"); //初始化自旋锁
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    {
      ref.refcount[(uint64)p / PGSIZE] = 1; //这里设置为1再kfree就变成0了
      kfree(p);
    }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  //释放内存时，引用计数大于0时只减少引用计数而不释放内存
  acquire(&ref.lock);
  if(--ref.refcount[(uint64)pa / PGSIZE] > 0) {
    release(&ref.lock);
    return;
  }
  release(&ref.lock);
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    {
      kmem.freelist = r->next;//分配页面时将其引用计数初始化为1
      //添加
      acquire(&ref.lock);
      ref.refcount[(uint64)r / PGSIZE] = 1; 
      release(&ref.lock);
      //结束
    }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
