#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "syscall.h"

// 共享内存区域数组
struct shm_region shm_regions[MAX_SHM_REGIONS];
struct spinlock shm_lock;

// 定义锁的获取顺序以避免死锁
// 顺序：进程锁(p->lock) -> 全局锁(shm_lock) -> 区域锁(region.lock)

// 初始化共享内存子系统
void
shm_init(void)
{
  initlock(&shm_lock, "shm");

  // 初始化所有共享内存区域
  for(int i = 0; i < MAX_SHM_REGIONS; i++) {
    shm_regions[i].used = 0;
    shm_regions[i].refcnt = 0;
    shm_regions[i].shmid = 0;
    shm_regions[i].marked_for_deletion = 0;  // 新增：标记是否等待删除
    initlock(&shm_regions[i].lock, "shm_region");
  }
}



// 通过shmid查找共享内存区域
// 注意：调用者必须先获取全局锁(shm_lock)
static struct shm_region*
shm_find_by_id_locked(int shmid)
{
  struct shm_region *region = 0;

  for(int i = 0; i < MAX_SHM_REGIONS; i++) {
    if(shm_regions[i].used && shm_regions[i].shmid == shmid) {
      region = &shm_regions[i];
      break;
    }
  }

  return region;
}



// 生成唯一的shmid
// 注意：调用者必须先获取全局锁(shm_lock)
static int
shm_generate_id_locked(void)
{
  static int next_id = 1;
  int id;

  id = next_id++;
  // 避免ID溢出，如果溢出则从1重新开始
  if(next_id <= 0) {
    next_id = 1;
  }

  return id;
}



// shmget: 创建或获取共享内存标识符
int
shmget(int key, int size, int shmflg)
{
  // 检查大小是否有效
  if(size <= 0 || size > PGSIZE)
    return -1;

  // 按照锁顺序，先获取全局锁
  acquire(&shm_lock);
  
  // 查找或创建共享内存区域
  int create = (shmflg & 0x01000) ? 1 : 0;  // IPC_CREAT
  struct shm_region *region = 0;
  struct shm_region *unused = 0;

  // 查找已存在的共享内存区域或空闲区域
  for(int i = 0; i < MAX_SHM_REGIONS; i++) {
    if(shm_regions[i].used && !shm_regions[i].marked_for_deletion) {
      if(shm_regions[i].key == key) {
        // 找到已存在的共享内存区域
        region = &shm_regions[i];
        break;
      }
    } else if(!unused && !shm_regions[i].used) {
      // 记录第一个未使用的区域
      unused = &shm_regions[i];
    }
  }

  // 如果没有找到且需要创建
  if(!region && create && unused) {
    region = unused;
    region->used = 1;
    region->key = key;
    region->size = size;
    region->refcnt = 0;
    region->marked_for_deletion = 0;
    region->pa = (uint64)kalloc();

    if(region->pa == 0) {
      // 分配物理内存失败
      region->used = 0;
      region = 0;
    } else {
      // 初始化内存
      memset((void*)region->pa, 0, size);
      // 设置名称
      safestrcpy(region->name, "shm", SHM_NAME_LEN);
    }
  }

  if(!region) {
    // 创建失败或不存在
    release(&shm_lock);
    return -1;
  }

  // 按照锁顺序，再获取区域锁
  acquire(&region->lock);
  
  // 如果还没有分配shmid，分配一个
  if(region->shmid == 0) {
    region->shmid = shm_generate_id_locked();
  }
  int shmid = region->shmid;
  
  // 释放锁
  release(&region->lock);
  release(&shm_lock);

  return shmid;
}

// shmat: 将共享内存附加到进程地址空间
void*
shmat(int shmid, const void *addr, int shmflg)
{
  struct proc *p = myproc();
  struct shm_region *region = 0;
  
  // 按照锁顺序，先获取进程锁
  acquire(&p->lock);
  
  // 查找一个空闲的附加区域
  int attach_idx = -1;
  for(int i = 0; i < MAX_SHM_ATTACH; i++) {
    if(!p->shm_attached[i].used) {
      attach_idx = i;
      break;
    }
  }

  if(attach_idx < 0) {
    // 没有空闲的附加区域
    release(&p->lock);
    return (void*)-1;
  }
  
  // 按照锁顺序，再获取全局锁
  acquire(&shm_lock);
  
  // 查找共享内存区域
  region = shm_find_by_id_locked(shmid);
  if(!region || region->marked_for_deletion) {
    release(&shm_lock);
    release(&p->lock);
    return (void*)-1;
  }
  
  // 确定附加地址
  uint64 va;
  if(addr == 0) {
    // 由内核选择地址
    va = p->sz;
  } else {
    // 使用指定的地址
    va = (uint64)addr;
  }

  // 确保地址是页对齐的
  va = PGROUNDDOWN(va);
  
  // 确保大小是页对齐的
  uint64 size = PGROUNDUP(region->size);
  
  // 检查地址是否已被映射
  pte_t *pte = walk(p->pagetable, va, 0);
  if(pte && (*pte & PTE_V)) {
    release(&shm_lock);
    release(&p->lock);
    return (void*)-1;  // 地址已被映射
  }

  // 按照锁顺序，最后获取区域锁
  acquire(&region->lock);

  // 增加引用计数
  region->refcnt++;

  // 映射物理内存到虚拟地址
  if(mappages(p->pagetable, va, size, region->pa, PTE_U | PTE_R | PTE_W) != 0) {
    region->refcnt--;
    release(&region->lock);
    release(&shm_lock);
    release(&p->lock);
    return (void*)-1;
  }

  // 记录附加信息
  p->shm_attached[attach_idx].used = 1;
  p->shm_attached[attach_idx].shmid = shmid;
  p->shm_attached[attach_idx].va = va;

  // 如果需要，更新进程大小
  if(va + size > p->sz) {
    p->sz = va + size;
  }

  release(&region->lock);
  release(&shm_lock);
  release(&p->lock);

  return (void*)va;
}

// shmdt: 从进程地址空间分离共享内存
int
shmdt(const void *addr)
{
  struct proc *p = myproc();
  int shmid = -1;
  int found = 0;

  // 按照锁顺序，先获取进程锁
  acquire(&p->lock);
  
  // 查找附加信息
  uint64 addr_val = (uint64)addr;
  for(int i = 0; i < MAX_SHM_ATTACH; i++) {
    if(p->shm_attached[i].used && p->shm_attached[i].va == addr_val) {
      shmid = p->shm_attached[i].shmid;
      p->shm_attached[i].used = 0;
      found = 1;
      break;
    }
  }

  if(!found) {
    release(&p->lock);
    return -1;
  }

  // 按照锁顺序，再获取全局锁
  acquire(&shm_lock);
  
  // 查找共享内存区域
  struct shm_region *region = shm_find_by_id_locked(shmid);
  if(!region) {
    release(&shm_lock);
    release(&p->lock);
    return -1;
  }
  
  // 按照锁顺序，最后获取区域锁
  acquire(&region->lock);

  // 取消映射页面
  uint64 size = PGROUNDUP(region->size);
  uvmunmap(p->pagetable, addr_val, size / PGSIZE, 0);

  // 减少引用计数
  region->refcnt--;
  
  // 检查是否需要删除共享内存区域
  int should_delete = 0;
  if(region->marked_for_deletion && region->refcnt == 0) {
    should_delete = 1;
  }

  // 释放区域锁和进程锁，但保留全局锁
  release(&region->lock);
  release(&p->lock);
  
  // 如果需要删除，使用已持有的全局锁
  if(should_delete) {
    // 再次检查，防止在等待锁期间状态改变
    if(region->marked_for_deletion && region->refcnt == 0) {
      kfree((void*)region->pa);
      region->used = 0;
      region->marked_for_deletion = 0;
    }
  }
  
  // 释放全局锁
  release(&shm_lock);

  return 0;
}

// shmctl: 共享内存控制操作
int
shmctl(int shmid, int cmd, void *buf)
{
  // 按照锁顺序，先获取全局锁
  acquire(&shm_lock);
  
  // 查找共享内存区域
  struct shm_region *region = shm_find_by_id_locked(shmid);
  if(!region) {
    release(&shm_lock);
    return -1;
  }

  switch(cmd) {
    case 1: {  // IPC_RMID - 删除共享内存
      // 按照锁顺序，再获取区域锁
      acquire(&region->lock);
      
      // 标记为删除，但实际删除推迟到引用计数为0时
      region->marked_for_deletion = 1;
      
      // 如果引用计数已经为0，立即删除
      if(region->refcnt == 0) {
        // 释放物理内存
        kfree((void*)region->pa);
        region->used = 0;
        region->marked_for_deletion = 0;
      }
      
      release(&region->lock);
      release(&shm_lock);
      return 0;
    }
    
    case 2: {  // 新增：获取共享内存信息
      struct shm_info {
        int shmid;
        int key;
        int size;
        int refcnt;
        int marked_for_deletion;
      } *info = (struct shm_info*)buf;
      
      if(!info) {
        release(&shm_lock);
        return -1;
      }
      
      // 按照锁顺序，再获取区域锁
      acquire(&region->lock);
      info->shmid = region->shmid;
      info->key = region->key;
      info->size = region->size;
      info->refcnt = region->refcnt;
      info->marked_for_deletion = region->marked_for_deletion;
      release(&region->lock);
      release(&shm_lock);
      
      return 0;
    }
    
    default:
      release(&shm_lock);
      return -1;
  }
}
