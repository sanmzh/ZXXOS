// Mutual exclusion spin locks.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "proc.h"
#include "defs.h"

// LAB_LOCK
#define NLOCK 500

static struct spinlock *locks[NLOCK];
struct spinlock lock_locks;

void
freelock(struct spinlock *lk)
{
  acquire(&lock_locks);
  int i;
  for (i = 0; i < NLOCK; i++) {
    if(locks[i] == lk) {
      locks[i] = 0;
      break;
    }
  }
  release(&lock_locks);
}

static void
findslot(struct spinlock *lk) {
  acquire(&lock_locks);
  int i;
  for (i = 0; i < NLOCK; i++) {
    if(locks[i] == 0) {
      locks[i] = lk;
      release(&lock_locks);
      return;
    }
  }
  panic("findslot");
}
// EDN LAB_LOCK

void
initlock(struct spinlock *lk, char *name)
{
  lk->name = name;
  lk->locked = 0;
  lk->cpu = 0;
// LAB_LOCK
  lk->nts = 0;
  lk->n = 0;
  findslot(lk);
// END LAB_LOCK
}

// Acquire the lock.
// Loops (spins) until the lock is acquired.
void
acquire(struct spinlock *lk)
{
  push_off(); // disable interrupts to avoid deadlock.
  if(holding(lk))
    panic("acquire");

// LAB_LOCK
    __sync_fetch_and_add(&(lk->n), 1);
// END LAB_LOCK

  // On RISC-V, sync_lock_test_and_set turns into an atomic swap:
  //   a5 = 1
  //   s1 = &lk->locked
  //   amoswap.w.aq a5, a5, (s1)
  while(__sync_lock_test_and_set(&lk->locked, 1) != 0) {
    // LAB_LOCK
    __sync_fetch_and_add(&(lk->nts), 1);
    // END LAB_LOCK
  }

  // Tell the C compiler and the processor to not move loads or stores
  // past this point, to ensure that the critical section's memory
  // references happen strictly after the lock is acquired.
  // On RISC-V, this emits a fence instruction.
  __sync_synchronize();

  // Record info about lock acquisition for holding() and debugging.
  lk->cpu = mycpu();
}

// Release the lock.
void
release(struct spinlock *lk)
{
  if(!holding(lk))
    panic("release");

  lk->cpu = 0;

  // Tell the C compiler and the CPU to not move loads or stores
  // past this point, to ensure that all the stores in the critical
  // section are visible to other CPUs before the lock is released,
  // and that loads in the critical section occur strictly before
  // the lock is released.
  // On RISC-V, this emits a fence instruction.
  __sync_synchronize();

  // Release the lock, equivalent to lk->locked = 0.
  // This code doesn't use a C assignment, since the C standard
  // implies that an assignment might be implemented with
  // multiple store instructions.
  // On RISC-V, sync_lock_release turns into an atomic swap:
  //   s1 = &lk->locked
  //   amoswap.w zero, zero, (s1)
  __sync_lock_release(&lk->locked);

  pop_off();
}

// LAB_LOCK
static void
read_acquire_inner(struct rwspinlock *rwlk)
{
  uint32 old_state, new_state;

  while (1) {
    // 首先检查是否有等待的写者
    uint32 writers_waiting = __atomic_load_n(&rwlk->writers, __ATOMIC_ACQUIRE);

    // 如果有等待的写者，必须等待
    if (writers_waiting > 0) {
      // 自旋等待直到没有写者
      do {
        __sync_synchronize(); // 内存屏障
        writers_waiting = __atomic_load_n(&rwlk->writers, __ATOMIC_ACQUIRE);
      } while (writers_waiting > 0);
      continue; // 写者消失后，从头开始
    }

    // 检查锁状态
    old_state = __atomic_load_n(&rwlk->state, __ATOMIC_ACQUIRE);

    // 如果写锁被持有，等待
    if (old_state & 1) {
      // 自旋等待写锁释放
      do {
        __sync_synchronize(); // 内存屏障
        old_state = __atomic_load_n(&rwlk->state, __ATOMIC_ACQUIRE);
      } while (old_state & 1);
      // 写锁释放后，从头开始检查新的写者
      continue;
    }

    // 最终检查是否有新的写者出现
    if (__atomic_load_n(&rwlk->writers, __ATOMIC_ACQUIRE) > 0) {
      continue; // 如果有写者出现，必须等待
    }

    // 尝试增加读者计数（加 2）
    new_state = old_state + 2;

    // 原子更新状态
    if (__atomic_compare_exchange_n(&rwlk->state, &old_state, new_state,
                                   0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
      break; // 成功获取读锁
    }

    // 如果失败，检查是否是因为写者出现
    if (__atomic_load_n(&rwlk->writers, __ATOMIC_ACQUIRE) > 0) {
      continue; // 如果有写者出现，从头开始
    }
  }
}

static void
read_release_inner(struct rwspinlock *rwlk)
{
  // 使用原子操作安全地减少读者计数
  uint32 old_state, new_state;

  do {
    old_state = __atomic_load_n(&rwlk->state, __ATOMIC_ACQUIRE);
    // 减少读者计数（减 2）
    new_state = old_state - 2;
  } while (!__atomic_compare_exchange_n(&rwlk->state, &old_state, new_state,
                                        0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE));
}

static void
write_acquire_inner(struct rwspinlock *rwlk)
{
  // 增加写者计数以表示意图
  __atomic_fetch_add(&rwlk->writers, 1, __ATOMIC_ACQ_REL);

  // 等待锁完全空闲（无读者或写者）
  uint32 old_state;
  do {
    old_state = __atomic_load_n(&rwlk->state, __ATOMIC_ACQUIRE);
    // 自旋等待锁被释放
    while (old_state != 0) {
      __sync_synchronize(); // 内存屏障
      old_state = __atomic_load_n(&rwlk->state, __ATOMIC_ACQUIRE);
    }
  } while (!__atomic_compare_exchange_n(&rwlk->state, &old_state, 1,
                                        0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE));

  // 减少写者计数，因为我们已经获取了锁
  __atomic_fetch_sub(&rwlk->writers, 1, __ATOMIC_ACQ_REL);
}

static void
write_release_inner(struct rwspinlock *rwlk)
{
  // 使用原子操作安全地释放写锁
  __atomic_store_n(&rwlk->state, 0, __ATOMIC_RELEASE);
}

void
read_acquire(struct rwspinlock *rwlk)
{
  push_off(); // disable interrupts to avoid deadlock.
  read_acquire_inner(rwlk);
}

void
read_release(struct rwspinlock *rwlk)
{
  read_release_inner(rwlk);
  pop_off();
}

void
write_acquire(struct rwspinlock *rwlk)
{
  push_off(); // disable interrupts to avoid deadlock.
  write_acquire_inner(rwlk);
}

void
write_release(struct rwspinlock *rwlk)
{
  write_release_inner(rwlk);
  pop_off();
}

void
initrwlock(struct rwspinlock *rwlk)
{
  initlock(&rwlk->lk, "rwlk");
  rwlk->state = 0;  // 初始状态：无读者，无写者
  rwlk->writers = 0;  // 初始无等待的写者
  rwlk->name = "rwlk";  // 设置名称
  findslot(&rwlk->lk);  // 注册锁
}

// Test rwspinlock implementation.
static void
rwspinlock_test_step(uint step, const char *msg)
{
  static uint barrier;
  const uint ncpu = 4;

  __atomic_fetch_add(&barrier, 1, __ATOMIC_ACQ_REL);
  while (__atomic_load_n(&barrier, __ATOMIC_RELAXED) < ncpu * step) {
    // spin
  }

  if (cpuid() == 0) {
    printf("rwspinlock_test: step %d: %s\n", step, msg);
  }
}

static uint
delay()
{
  static uint v;
  for (int i = 0; i < 10000; i++) {
    __atomic_fetch_add(&v, 1, __ATOMIC_RELAXED);
  }
  return __atomic_load_n(&v, __ATOMIC_RELAXED);
}

uint64
sys_rwlktest()
{
  int r = 0;
  int step = 0;

  push_off();
  int id = cpuid();

  rwspinlock_test_step(++step, "initrwlock");

  static struct rwspinlock l;
  if (id == 0) {
    initrwlock(&l);
  }

  rwspinlock_test_step(++step, "concurrent read_acquire");

  for (int i = 0; i < 1000000; i++)
    read_acquire(&l);

  rwspinlock_test_step(++step, "concurrent read_release");

  for (int i = 0; i < 1000000; i++)
    read_release(&l);

  rwspinlock_test_step(++step, "prepare read_acquire for writer priority test");

  if (id == 1) {
    for (int i = 0; i < 30; i++) {
      read_acquire(&l);
    }
  }

  rwspinlock_test_step(++step, "writer priority test");

  static uint flag;
  if (id == 0) {
    write_acquire(&l);
    __atomic_store_n(&flag, 1, __ATOMIC_RELAXED);
    write_release(&l);
  }

  if (id == 1) {
    delay();
    for (int i = 0; i < 10; i++) {
      read_release(&l);
    }
    delay();
    for (int i = 0; i < 10; i++) {
      read_release(&l);
    }
    delay();
    for (int i = 0; i < 10; i++) {
      read_release(&l);
    }
  }

  if (id == 2) {
    delay();
    read_acquire(&l);
    uint f = __atomic_load_n(&flag, __ATOMIC_RELAXED);
    if (f == 0) {
      printf("rwspinlock_test: reader sneaked ahead of waiting writer\n");
      r = -1;
    }
    read_release(&l);
  }

  rwspinlock_test_step(++step, "checking for concurrent readers/writers");

  static uint v;
  if (id == 0) {
    uint maxwv = 0;
    for (int i = 0; i < 1000000; i++) {
      write_acquire(&l);
      uint x = __atomic_add_fetch(&v, 1, __ATOMIC_ACQ_REL);
      if (x > maxwv) {
        maxwv = x;
      }
      uint y = __atomic_fetch_sub(&v, 1, __ATOMIC_ACQ_REL);
      if (y > maxwv) {
        maxwv = y;
      }
      write_release(&l);
    }
    if (maxwv > 1) {
      printf("rwspinlock_test: cpu %d saw concurrent reads/writes: %d\n", id, maxwv);
      r = -1;
    }
  } else {
    uint maxrv = 0;
    for (int i = 0; i < 1000000; i++) {
      read_acquire(&l);
      uint x = __atomic_add_fetch(&v, 1, __ATOMIC_ACQ_REL);
      if (x > maxrv) {
        maxrv = x;
      }
      uint y = __atomic_fetch_sub(&v, 1, __ATOMIC_ACQ_REL);
      if (y > maxrv) {
        maxrv = y;
      }
      read_release(&l);
    }
    if (maxrv < 2) {
      printf("rwspinlock_test: cpu %d never saw concurrent reads: %d\n", id, maxrv);
      r = -1;
    }
  }

  rwspinlock_test_step(++step, "checking for concurrent writers");

  uint maxwv = 0;
  for (int i = 0; i < 1000000; i++) {
    write_acquire(&l);
    uint x = __atomic_add_fetch(&v, 1, __ATOMIC_ACQ_REL);
    if (x > maxwv) {
      maxwv = x;
    }
    uint y = __atomic_fetch_sub(&v, 1, __ATOMIC_ACQ_REL);
    if (y > maxwv) {
      maxwv = y;
    }
    write_release(&l);
  }
  if (maxwv > 1) {
    printf("rwspinlock_test: cpu %d saw concurrent writes: %d\n", id, maxwv);
    r = -1;
  }

  rwspinlock_test_step(++step, "acquiring multiple locks");

  struct rwspinlock l2;
  initrwlock(&l2);
  write_acquire(&l2);
  read_acquire(&l);

  rwspinlock_test_step(++step, "releasing multiple locks");

  write_release(&l2);
  read_release(&l);

  for (int i = 0; i < 10; i++) {
    rwspinlock_test_step(++step, "prepare read_acquire for multiple writer priority test");

    static uint writer_count;
    if (id == 3) {
      writer_count = 0;
      read_acquire(&l);
      read_acquire(&l);
    }

    rwspinlock_test_step(++step, "multiple writer priority test");

    if (id == 0 || id == 1) {
      write_acquire(&l);
      writer_count++;
      delay();
      write_release(&l);
    }

    if (id == 2) {
      delay();
      read_acquire(&l);
      if (writer_count == 0) {
        printf("rwspinlock_test: reader sneaked ahead of both waiting writers\n");
        r = -1;
      }
      delay();
      delay();
      delay();
      read_release(&l);
    }

    if (id == 3) {
      delay();
      read_release(&l);
      delay();
      read_release(&l);

      delay();
      delay();

      // By this point, either one writer executed and CPU 2 is holding read lock,
      // or both writers executed.  Should never sneak ahead of second writer.
      read_acquire(&l);
      if (writer_count != 2) {
        printf("rwspinlock_test: reader sneaked ahead of second waiting writer\n");
        r = -1;
      }
      read_release(&l);
    }
  }

  rwspinlock_test_step(++step, "done");

  printf("rwspinlock_test(%d): %d\n", id, r);
  pop_off();

  return r;
}
// END LAB_LOCK

// Check whether this cpu is holding the lock.
// Interrupts must be off.
int
holding(struct spinlock *lk)
{
  int r;
  r = (lk->locked && lk->cpu == mycpu());
  return r;
}

// push_off/pop_off are like intr_off()/intr_on() except that they are matched:
// it takes two pop_off()s to undo two push_off()s.  Also, if interrupts
// are initially off, then push_off, pop_off leaves them off.

void
push_off(void)
{
  int old = intr_get();

  // disable interrupts to prevent an involuntary context
  // switch while using mycpu().
  intr_off();

  if(mycpu()->noff == 0)
    mycpu()->intena = old;
  mycpu()->noff += 1;
}

void
pop_off(void)
{
  struct cpu *c = mycpu();
  if(intr_get())
    panic("pop_off - interruptible");
  if(c->noff < 1)
    panic("pop_off");
  c->noff -= 1;
  if(c->noff == 0 && c->intena)
    intr_on();
}

// Read a shared 32-bit value without holding a lock
int
atomic_read4(int *addr) {
  uint32 val;
  __atomic_load(addr, &val, __ATOMIC_SEQ_CST);
  return val;
}

// LAB_LOCK
int
snprint_lock(char *buf, int sz, struct spinlock *lk)
{
  int n = 0;
  if(lk->n > 0) {
    n = snprintf(buf, sz, "lock: %s: #test-and-set %d #acquire() %d\n",
                 lk->name, lk->nts, lk->n);
  }
  return n;
}

int
statslock(char *buf, int sz) {
  int n;
  int tot = 0;

  acquire(&lock_locks);
  n = snprintf(buf, sz, "--- lock kmem stats\n");
  for(int i = 0; i < NLOCK; i++) {
    if(locks[i] == 0)
      break;
    if(strncmp(locks[i]->name, "kmem", strlen("kmem")) == 0) {
      tot += locks[i]->nts;
      n += snprint_lock(buf +n, sz-n, locks[i]);
    }
  }
  
  n += snprintf(buf+n, sz-n, "--- top 5 contended locks:\n");
  int last = 100000000;
  // stupid way to compute top 5 contended locks
  for(int t = 0; t < 5; t++) {
    int top = 0;
    for(int i = 0; i < NLOCK; i++) {
      if(locks[i] == 0)
        break;
      if(locks[i]->nts > locks[top]->nts && locks[i]->nts < last) {
        top = i;
      }
    }
    n += snprint_lock(buf+n, sz-n, locks[top]);
    last = locks[top]->nts;
  }
  n += snprintf(buf+n, sz-n, "tot= %d\n", tot);
  release(&lock_locks);  
  return n;
}
// END LAB_LOCK