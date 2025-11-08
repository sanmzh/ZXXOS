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

// 引用计数数组
struct {
  struct spinlock lock;
  int ref_count[(PHYSTOP - KERNBASE) / PGSIZE];
} refcnt;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&refcnt.lock, "refcnt");
  
  // 初始化引用计数
  acquire(&refcnt.lock);
  for(int i = 0; i < (PHYSTOP - KERNBASE) / PGSIZE; i++) {
    refcnt.ref_count[i] = 0;
  }
  release(&refcnt.lock);
  
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // 减少引用计数
  dec_refcnt(pa);
  
  // 只有引用计数为0时才真正释放
  if(get_refcnt(pa) > 0)
    return;

  // Fill with junk to catch dangling refs.
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
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    // 初始化引用计数为1
    acquire(&refcnt.lock);
    int idx = ((uint64)r - KERNBASE) / PGSIZE;
    refcnt.ref_count[idx] = 1;
    release(&refcnt.lock);
  }
  
  // 确保页表页面的引用计数也被正确初始化
  // 这对于COW实现很重要
  return (void*)r;
}

// 获取页面对应的引用计数
int
get_refcnt(void *pa)
{
  int idx = ((uint64)pa - KERNBASE) / PGSIZE;
  acquire(&refcnt.lock);
  int count = refcnt.ref_count[idx];
  release(&refcnt.lock);
  return count;
}

// 增加引用计数
void
inc_refcnt(void *pa)
{
  int idx = ((uint64)pa - KERNBASE) / PGSIZE;
  acquire(&refcnt.lock);
  refcnt.ref_count[idx]++;
  release(&refcnt.lock);
}

// 减少引用计数
void
dec_refcnt(void *pa)
{
  int idx = ((uint64)pa - KERNBASE) / PGSIZE;
  acquire(&refcnt.lock);
  refcnt.ref_count[idx]--;
  release(&refcnt.lock);
}

// 获取空闲内存
void
freebytes(uint64* dst)
{
  *dst = 0;
  struct run *r = kmem.freelist;

  acquire(&kmem.lock);      // 获取锁，防止其他线程修改
  while(r) {
    *dst += PGSIZE;         // 累加空闲内存大小
    r = r->next;
  }
  release(&kmem.lock);      // 释放锁
}