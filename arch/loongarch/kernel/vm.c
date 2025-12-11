#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "loongarch.h"
#include "defs.h"
#include "fs.h"
#include "spinlock.h"
#include "proc.h"

void
tlbinit(void)
{
  asm volatile("invtlb  0x0,$zero,$zero");
  w_csr_stlbps(0xcU);
  w_csr_asid(0x0U);//todo
  w_csr_tlbrehi(0xcU);
}

void
vminit(void)//todo
{
  pagetable_t kpgtbl;

  kpgtbl = (pagetable_t) kalloc();
  memset(kpgtbl, 0, PGSIZE);
  proc_mapstacks(kpgtbl);

  w_csr_pgdl((uint64)kpgtbl);
  tlbinit();

  w_csr_pwcl((PTEWIDTH << 30)|(DIR2WIDTH << 25)|(DIR2BASE << 20)|(DIR1WIDTH << 15)|(DIR1BASE << 10)|(PTWIDTH << 5)|(PTBASE << 0));
  w_csr_pwch((DIR4WIDTH << 18)|(DIR3WIDTH << 6)|(DIR3BASE << 0));
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The paging scheme has four levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   48..63 -- must be zero.
//   39..47 -- 9 bits of level-3 index.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
  {
    panic("walk");
  }
  for(int level = 3; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      pagetable = (pagetable_t)(PTE2PA(*pte) | DMWIN_MASK);      
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if(va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if(pte == 0)
    return 0;
  if((*pte & PTE_V) == 0)
    return 0;
  if((*pte & PTE_PLV) == 0)
    return 0;
  pa = PTE2PA(*pte);
  return pa;
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, uint64 perm)
{
  uint64 a, last;
  pte_t *pte;

  if(size == 0)
    panic("mappages: size");
  
  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);
  for(;;){
    if((pte = walk(pagetable, a, 1)) == 0)
      return -1;
    if(*pte & PTE_V)
      panic("mappages: remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");
  
  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0)
      panic("uvmunmap: walk");
    if((*pte & PTE_V) == 0)
      panic("uvmunmap: not mapped");
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");
    if(do_free){
      uint64 pa = PTE2PA(*pte);    
      kfree((void*)(pa | DMWIN_MASK));
    }
    *pte = 0;
  }
}

// create an empty user page table.
// returns 0 if out of memory.
pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if(pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// Load the user initcode into address 0 of pagetable,
// for the very first process.
// sz must be less than a page.
void
uvminit(pagetable_t pagetable, uchar *src, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_P|PTE_W|PTE_PLV|PTE_MAT);
  memmove(mem, src, sz);
}//todo

// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  char *mem;
  uint64 a;

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_P|PTE_W|PTE_PLV|PTE_MAT|PTE_D) != 0){
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if(newsz >= oldsz)
    return oldsz;

  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

// Recursively free page-table pages.
// All leaf mappings must already have been removed.
void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (PTE_FLAGS(pte) == PTE_V)){
      // this PTE points to a lower-level page table.
      uint64 child = (PTE2PA(pte) | DMWIN_MASK);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      panic("freewalk: leaf");
    }
  }
  kfree((void*)pagetable);
}

// Free user memory pages,
// then free page-table pages.
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  freewalk(pagetable);
}

// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);

    // 增加引用计数
    inc_refcnt((void*)pa);
    
    // 如果页面是可写的，清除父子进程的写权限并标记为 COW
    // 如果页面已经是COW页面，则不需要再次处理
    if(flags & PTE_W && !(flags & PTE_COW)) {
      flags = ((flags & ~PTE_W) | PTE_COW) & ~PTE_D;
      // 同时更新父进程的页表项
      *pte = PA2PTE(pa) | flags;
    }
    
    // 在子进程页表中映射相同的物理页
    if(mappages(new, i, PGSIZE, (uint64)pa, flags) != 0){
      // 如果映射失败，减少引用计数
      dec_refcnt((void*)pa);
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  
  pte = walk(pagetable, va, 0);
  if(pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_PLV;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;
  pte_t *pte;
  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if(va0>=MAXVA)
    {
      return -1;
    }
    // 检查地址是否在进程的有效地址范围内
    // 注意：在exec过程中，pagetable可能不是当前进程的pagetable，
    // 所以需要特殊处理这种情况
    struct proc *curproc = myproc();
    if(curproc && curproc->pagetable == pagetable && va0 >= curproc->sz)
      return -1;

    pte = walk(pagetable, va0, 0);
    if(pa0 == 0)
      return -1;
    // 检查是否是 COW 页面
    if((*pte & PTE_COW) && (*pte & PTE_W) == 0) {
      // 处理 COW 页面
      if(cow_handler(pagetable, va0) < 0)
        return -1;
      if(va0>=MAXVA)
      {
      return -1;
      }
      // 重新获取页表项，因为 cow_handler 可能已经修改了它
      pte = walk(pagetable, va0, 0);
      if(pte == 0 || (*pte & PTE_V) == 0)
        return -1;
      // 更新 pa0，因为 cow_handler 可能已经修改了页表项
      // 检查地址是否在进程的有效地址范围内
      // 注意：在exec过程中，pagetable可能不是当前进程的pagetable，
      // 所以需要特殊处理这种情况
      struct proc *curproc = myproc();
      if(curproc && curproc->pagetable == pagetable && va0 >= curproc->sz)
        return -1;
    
      pa0 = PTE2PA(*pte);
    }
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)((pa0 + (dstva - va0)) | DMWIN_MASK), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}//todo

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > len)
      n = len;
    memmove(dst, (void *)((pa0 + (srcva - va0)) | DMWIN_MASK), n);

    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
}//todo

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  uint64 n, va0, pa0;
  int got_null = 0;

  while(got_null == 0 && max > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > max)
      n = max;

    char *p = (char *) ((pa0 + (srcva - va0)) | DMWIN_MASK);
    while(n > 0){
      if(*p == '\0'){
        *dst = '\0';
        got_null = 1;
        break;
      } else {
        *dst = *p;
      }
      --n;
      --max;
      p++;
      dst++;
    }

    srcva = va0 + PGSIZE;
  }
  if(got_null){
    return 0;
  } else {
    return -1;
  }
}//todo

// 处理 COW 页错误
int
cow_handler(pagetable_t pagetable, uint64 va)
{
  if(va >= MAXVA)
    return -1;

  va = PGROUNDDOWN(va); // 向下取整
  if(va>=MAXVA)
  {
      return -1;
  }
  pte_t *pte = walk(pagetable, va, 0);
  if(pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_COW) == 0)
    return -1;
  
  uint64 pa = PTE2PA(*pte);
  uint flags = PTE_FLAGS(*pte);
  
  // 如果引用计数为 1，则直接写
  if(get_refcnt((void*)pa) == 1) {
    flags =((flags & ~PTE_COW) | PTE_W) | PTE_D;   // 更新页表项，设置为可写并清除 COW 标记
    *pte = PA2PTE(pa) | flags;            // 更新页表项，映射到新页面并设置为可写
    return 0;
  }
  // 否则，分配新页面
  uint64 new_pa = (uint64)kalloc();
  if(new_pa == 0) {
    // 内存分配失败，杀死进程
    struct proc *p = myproc();
    if(p) {
      printf("cow_handler: out of memory, killing process %d\n", p->pid);
      p->killed=1;
      return -1;

    }
    return -1;
  }
  
  // 复制页面内容
  memmove((void*)new_pa, ((void*)(pa | DMWIN_MASK)), PGSIZE);
  
  // 减少原页面的引用计数
  dec_refcnt((void*)pa);
  
  // 更新页表项，设置为可写并清除 COW 标记
  flags = ((flags & ~PTE_COW) | PTE_W) | PTE_D;
  *pte = PA2PTE(new_pa) | flags;
  
  // 刷新 TLB
  tlb_flush_all();
  return 0;
}

