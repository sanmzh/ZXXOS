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

// 初始化共享内存子系统
void
shm_init(void)
{
  initlock(&shm_lock, "shm");

  // 初始化所有共享内存区域
  for(int i = 0; i < MAX_SHM_REGIONS; i++) {
    shm_regions[i].used = 0;
    initlock(&shm_regions[i].lock, "shm_region");
  }
}

// 查找或创建一个共享内存区域
// key: 键值
// size: 大小
// create: 是否创建
// 返回: 共享内存区域指针，失败返回NULL
static struct shm_region*
shm_find_or_create(int key, int size, int create)
{
  struct shm_region *region = 0;
  struct shm_region *unused = 0;

  acquire(&shm_lock);

  // 查找已存在的共享内存区域或空闲区域
  for(int i = 0; i < MAX_SHM_REGIONS; i++) {
    if(shm_regions[i].used) {
      if(shm_regions[i].key == key) {
        // 找到已存在的共享内存区域
        region = &shm_regions[i];
        break;
      }
    } else if(!unused) {
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

  release(&shm_lock);

  return region;
}

// 通过shmid查找共享内存区域
static struct shm_region*
shm_find_by_id(int shmid)
{
  struct shm_region *region = 0;

  acquire(&shm_lock);

  for(int i = 0; i < MAX_SHM_REGIONS; i++) {
    if(shm_regions[i].used && shm_regions[i].shmid == shmid) {
      region = &shm_regions[i];
      break;
    }
  }

  release(&shm_lock);

  return region;
}

// 生成唯一的shmid
static int
shm_generate_id(void)
{
  static int next_id = 1;
  int id;

  acquire(&shm_lock);
  id = next_id++;
  release(&shm_lock);

  return id;
}

// shmget: 创建或获取共享内存标识符
int
shmget(int key, int size, int shmflg)
{
  // 检查大小是否有效
  if(size <= 0 || size > PGSIZE)
    return -1;

  // 查找或创建共享内存区域
  int create = (shmflg & 0x01000) ? 1 : 0;  // IPC_CREAT
  struct shm_region *region = shm_find_or_create(key, size, create);

  if(!region) {
    // 创建失败或不存在
    return -1;
  }

  // 如果还没有分配shmid，分配一个
  acquire(&region->lock);
  if(region->shmid == 0) {
    region->shmid = shm_generate_id();
  }
  int shmid = region->shmid;
  release(&region->lock);

  return shmid;
}

// shmat: 将共享内存附加到进程地址空间
void*
shmat(int shmid, const void *addr, int shmflg)
{

  // 查找共享内存区域
  struct shm_region *region = shm_find_by_id(shmid);
  if(!region) {
    return (void*)-1;
  }

  struct proc *p = myproc();

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

  // 映射共享内存到进程地址空间
  acquire(&region->lock);

  // 增加引用计数
  region->refcnt++;

  // 映射物理内存到虚拟地址
  if(mappages(p->pagetable, va, region->size, region->pa, PTE_U | PTE_R | PTE_W) != 0) {
    region->refcnt--;
    release(&region->lock);
    return (void*)-1;
  }

  // 记录附加信息
  p->shm_attached[attach_idx].used = 1;
  p->shm_attached[attach_idx].shmid = shmid;
  p->shm_attached[attach_idx].va = va;

  // 如果需要，更新进程大小
  if(va + region->size > p->sz) {
    p->sz = va + region->size;
  }

  release(&region->lock);

  return (void*)va;
}

// shmdt: 从进程地址空间分离共享内存
int
shmdt(const void *addr)
{

  struct proc *p = myproc();
  int shmid = -1;
  int found = 0;

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
    return -1;
  }

  // 查找共享内存区域
  struct shm_region *region = shm_find_by_id(shmid);
  if(!region) {
    return -1;
  }

  // 取消映射
  acquire(&region->lock);

  // 取消映射页面
  uvmunmap(p->pagetable, addr_val, region->size / PGSIZE, 0);

  // 减少引用计数
  region->refcnt--;

  // 如果引用计数为0，释放物理内存
  if(region->refcnt == 0) {
    kfree((void*)region->pa);
    region->used = 0;
  }

  release(&region->lock);

  return 0;
}

// shmctl: 共享内存控制操作
int
shmctl(int shmid, int cmd, void *buf)
{

  // 查找共享内存区域
  struct shm_region *region = shm_find_by_id(shmid);
  if(!region) {
    return -1;
  }

  switch(cmd) {
    case 1:  // IPC_RMID - 删除共享内存
      acquire(&region->lock);
      if(region->refcnt == 0) {
        // 如果没有进程附加，立即删除
        kfree((void*)region->pa);
        region->used = 0;
      } else {
        // 标记为删除，当引用计数为0时删除
        // 这里简化处理，实际应该添加标记
      }
      release(&region->lock);
      return 0;

    default:
      return -1;
  }
}
