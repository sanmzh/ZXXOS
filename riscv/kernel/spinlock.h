// Mutual exclusion lock.
struct spinlock {
  uint locked;       // Is the lock held?

  // For debugging:
  char *name;        // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock.
// LAB_LOCK
  int nts;
  int n;
// END LAB_LOCK
};

// LAB_LOCK
// Reader-writer lock.
struct rwspinlock {
  struct spinlock lk;    // 内部自旋锁，用于保护状态
  uint32 state;          // 锁状态：bit 0 = 写锁，bits 1-31 = 读者计数
  uint32 writers;        // 等待的写者数量
  char *name;            // 锁的名称
};
// END LAB_LOCK