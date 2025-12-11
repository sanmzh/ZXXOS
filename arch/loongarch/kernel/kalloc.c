// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "loongarch.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

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
  int ref_count[(RAMSTOP - RAMBASE) / PGSIZE];
} refcnt;


void
kinit()
{
  initlock(&kmem.lock, "kmem");
  
  initlock(&refcnt.lock, "refcnt");
  
  // 初始化引用计数
  acquire(&refcnt.lock);
  for(int i = 0; i < (RAMSTOP - RAMBASE) / PGSIZE; i++) {
    refcnt.ref_count[i] = 0;
  }
  release(&refcnt.lock);


  freerange((void*)RAMBASE, (void*)RAMSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  push_off();
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    struct run *r;

    // Fill with junk to catch dangling refs.
    memset(p, 1, PGSIZE);

    r = (struct run*)p;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }

  pop_off();

}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (uint64)pa < RAMBASE || (uint64)pa >= RAMSTOP)
    panic("kfree");

  // 获取当前引用计数
  acquire(&refcnt.lock);
  int idx = ((uint64)pa - RAMBASE) / PGSIZE;
  if(refcnt.ref_count[idx] > 0) {
    refcnt.ref_count[idx]--;
    if(refcnt.ref_count[idx] > 0) {
      release(&refcnt.lock);
      return;
    }
  } else if(refcnt.ref_count[idx] < 0) {
    // 引用计数为负数，重置为0并打印警告
    printf("kfree: warning: refcnt is negative for pa %p, resetting to 0", pa);
    refcnt.ref_count[idx] = 0;
  }
  release(&refcnt.lock);


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
    int idx = ((uint64)r - RAMBASE) / PGSIZE;
    refcnt.ref_count[idx] = 1;
    release(&refcnt.lock);
  }
  
  // 确保页表页面的引用计数也被正确初始化
  // 这对于COW实现很重要

  return (void*)r;
}

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

// 获取页面对应的引用计数
int
get_refcnt(void *pa)
{
  int idx = ((uint64)pa - RAMBASE) / PGSIZE;
  acquire(&refcnt.lock);
  int count = refcnt.ref_count[idx];
  release(&refcnt.lock);
  return count;
}

// 增加引用计数
void
inc_refcnt(void *pa)
{
  int idx = ((uint64)pa - RAMBASE) / PGSIZE;
  acquire(&refcnt.lock);
  refcnt.ref_count[idx]++;
  release(&refcnt.lock);
}

// 减少引用计数
void
dec_refcnt(void *pa)
{
  int idx = ((uint64)pa - RAMBASE) / PGSIZE;
  acquire(&refcnt.lock);
  refcnt.ref_count[idx]--;
  if(refcnt.ref_count[idx] == 0) {
    release(&refcnt.lock);
    // 引用计数为0，直接释放页面到空闲列表
    // 不调用kfree，避免递归
    struct run *r = (struct run*)pa;
    memset(pa, 1, PGSIZE); // Fill with junk to catch dangling refs.

    push_off();
    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
    pop_off();
    return;
  }
  release(&refcnt.lock);
}

