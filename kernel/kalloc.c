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
} kmem[NCPU]; // 每个CPU一个空闲页链表

// 引用计数数组
struct {
  struct spinlock lock;
  int ref_count[(PHYSTOP - KERNBASE) / PGSIZE];
} refcnt;

void
kinit()
{
  char lock_name[16];
  for (int i = 0; i < NCPU; i++) {
    snprintf(lock_name, sizeof(lock_name), "kmem_%d", i);
    initlock(&kmem[i].lock, lock_name);
  }
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

  push_off();
  int cpu_id = cpuid();

  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    struct run *r;

    // Fill with junk to catch dangling refs.
    memset(p, 1, PGSIZE);

    r = (struct run*)p;

    acquire(&kmem[cpu_id].lock);
    r->next = kmem[cpu_id].freelist;
    kmem[cpu_id].freelist = r;
    release(&kmem[cpu_id].lock);
  }

  pop_off();
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

  // 获取当前引用计数
  acquire(&refcnt.lock);
  int idx = ((uint64)pa - KERNBASE) / PGSIZE;
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
  
  // 引用计数已经为0，继续释放页面

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  int cpu_id = cpuid();
  acquire(&kmem[cpu_id].lock);
  r->next = kmem[cpu_id].freelist;
  kmem[cpu_id].freelist = r;
  release(&kmem[cpu_id].lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int cpu_id = cpuid();

  acquire(&kmem[cpu_id].lock);
  r = kmem[cpu_id].freelist;
  if(r) {
    kmem[cpu_id].freelist = r->next;
    release(&kmem[cpu_id].lock);
    pop_off();
  } else {
    // 当前CPU的空闲列表为空，尝试从其他CPU窃取
    release(&kmem[cpu_id].lock);

    // 尝试从其他CPU获取内存
    for(int i = 0; i < NCPU; i++) {
      if(i == cpu_id) continue;

      acquire(&kmem[i].lock);
      r = kmem[i].freelist;
      if(r) {
        // 从其他CPU的列表中取走一半的内存块
        int count = 0;
        struct run *p = r;
        while(p) {
          count++;
          p = p->next;
        }

        // 取走一半
        int steal = count / 2;
        if(steal == 0) steal = 1; // 至少取走一个

        p = r;
        for(int j = 0; j < steal - 1; j++) {
          p = p->next;
        }

        kmem[i].freelist = p->next;
        p->next = 0;

        // 将窃取的内存放入当前CPU的列表
        acquire(&kmem[cpu_id].lock);
        kmem[cpu_id].freelist = r;
        r = kmem[cpu_id].freelist;
        kmem[cpu_id].freelist = r->next;
        release(&kmem[cpu_id].lock);

        release(&kmem[i].lock);
        break;
      }
      release(&kmem[i].lock);
    }
    pop_off();
  }

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    // 初始化引用计数为1
    acquire(&refcnt.lock);
    refcnt.ref_count[((uint64)r - KERNBASE) / PGSIZE] = 1;
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
  if(refcnt.ref_count[idx] == 0) {
    release(&refcnt.lock);
    // 引用计数为0，直接释放页面到空闲列表
    // 不调用kfree，避免递归
    struct run *r = (struct run*)pa;
    memset(pa, 1, PGSIZE); // Fill with junk to catch dangling refs.

    push_off();
    int cpu_id = cpuid();
    acquire(&kmem[cpu_id].lock);
    r->next = kmem[cpu_id].freelist;
    kmem[cpu_id].freelist = r;
    release(&kmem[cpu_id].lock);
    pop_off();
    return;
  }
  release(&refcnt.lock);
}

// 获取空闲内存
void
freebytes(uint64* dst)
{
  *dst = 0;

  // 遍历所有CPU的空闲列表
  for(int i = 0; i < NCPU; i++) {
    acquire(&kmem[i].lock);      // 获取当前CPU的锁
    struct run *r = kmem[i].freelist;
    while(r) {
      *dst += PGSIZE;           // 累加空闲内存大小
      r = r->next;
    }
    release(&kmem[i].lock);      // 释放当前CPU的锁
  }
}