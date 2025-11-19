#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "proc.h"
#include "fs.h"
#include "file.h"
#include "defs.h"
#include "fcntl.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

//
// handle an interrupt, exception, or system call from user space.
// called from, and returns to, trampoline.S
// return value is user satp for trampoline.S to switch to.
//
uint64
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);  //DOC: kernelvec

  struct proc *p = myproc();
  
  // save user program counter.
  p->trapframe->epc = r_sepc();
  
  if(r_scause() == 8){
    // system call

    if(killed(p))
      kexit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sepc, scause, and sstatus,
    // so enable only now that we're done with those registers.
    intr_on();

    syscall();
  } else if((which_dev = devintr()) != 0){
    // ok
  } else if(r_scause() == 15 || r_scause() == 13) {
    // 处理写错误（可能是COW页面）或读错误（懒分配页面）
    uint64 va = r_stval();
    uint64 cause = r_scause();
    
    // 如果是写错误，尝试处理COW页面
    if(cause == 15) {
      pte_t *pte = walk(p->pagetable, va, 0);
      if(pte && (*pte & PTE_V) && (*pte & PTE_COW)) {
        if(cow_handler(p->pagetable, va) == 0) {
          // COW处理成功
          goto done;
        } else {
          // COW处理失败，可能是内存不足，杀死进程
          setkilled(p);
          goto done;
        }
      }
    }
    
    // 如果不是COW页面或者COW处理失败，检查是否是mmap区域
    int mmap_result = mmap_handler(va, cause);
    if(mmap_result == 0) {
      // mmap处理成功
      goto done;
    }

    // 如果不是mmap区域或者mmap处理失败，尝试懒分配
    uint64 vmfault_result = vmfault(p->pagetable, va, (cause == 13)? 1 : 0);
    
    if(vmfault_result != 0) {
      // vmfault成功，页面已分配
      goto done;
    } else {
      // vmfault返回0，需要区分不同情况
      if(va >= MAXVA || va >= KERNBASE) {
        // 访问内核地址空间或超过MAXVA的地址，杀死进程
        printf("usertrap(): invalid address va=%p, pid=%d\n", (void*)va, p->pid);
        setkilled(p);
      } else if(va >= p->sz) {
        // 超出进程大小限制，杀死进程
        printf("usertrap(): address beyond process size va=%p, pid=%d\n", (void*)va, p->pid);
        setkilled(p);
      } else {
        // 其他情况，可能是内存不足或其他错误，杀死进程
        printf("usertrap(): page fault at va=%p, pid=%d\n", (void*)va, p->pid);
        printf("            sepc=0x%lx scause=0x%lx\n", r_sepc(), cause);
        setkilled(p);
      }
    }
  } else {
    printf("usertrap(): unexpected scause 0x%lx pid=%d\n", r_scause(), p->pid);
    printf("            sepc=0x%lx stval=0x%lx\n", r_sepc(), r_stval());
    setkilled(p);
  }

done:
  if(killed(p))
    kexit(-1);

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2)
    yield();

  prepare_return();

  // the user page table to switch to, for trampoline.S
  uint64 satp = MAKE_SATP(p->pagetable);

  // return to trampoline.S; satp value in a0.
  return satp;
}

//
// set up trapframe and control registers for a return to user space
//
void
prepare_return(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(). because a trap from kernel
  // code to usertrap would be a disaster, turn off interrupts.
  intr_off();

  // send syscalls, interrupts, and exceptions to uservec in trampoline.S
  uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
  w_stvec(trampoline_uservec);

  // set up trapframe values that uservec will need when
  // the process next traps into the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    // interrupt or trap from an unknown source
    printf("scause=0x%lx sepc=0x%lx stval=0x%lx\n", scause, r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2 && myproc() != 0)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  if(cpuid() == 0){
    acquire(&tickslock);
    ticks++;
    wakeup(&ticks);
    release(&tickslock);
  }

  // ask for the next timer interrupt. this also clears
  // the interrupt request. 1000000 is about a tenth
  // of a second.
  w_stimecmp(r_time() + 1000000);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr()
{
  uint64 scause = r_scause();

  if(scause == 0x8000000000000009L){
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } 
// LAB_NET
    else if (irq == E1000_IRQ) {
      e1000_intr();
    }
// END LAB_NET
    else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000005L){
    // timer interrupt.
    clockintr();
    return 2;
  } else {
    return 0;
  }
}

/**
 * @brief mmap_handler 处理mmap惰性分配导致的页面错误
 * @param va 页面故障虚拟地址
 * @param cause 页面故障原因
 * @return 0成功，-1失败
 */
int mmap_handler(int va, int cause)
{
  int i;
  struct proc *p = myproc();
  // 根据地址查找属于哪一个VMA
  for (i = 0; i < NVMA; ++i)
  {
    if (p->vma[i].used && p->vma[i].addr <= va && va <= p->vma[i].addr + p->vma[i].len - 1)
    {
      break;
    }
  }
  if (i == NVMA)
    return -1;

  int pte_flags = PTE_U;
  if (p->vma[i].prot & PROT_READ)
    pte_flags |= PTE_R;
  if (p->vma[i].prot & PROT_WRITE)
    pte_flags |= PTE_W;
  if (p->vma[i].prot & PROT_EXEC)
    pte_flags |= PTE_X;

  struct file *vf = p->vma[i].vfile;
  // 读导致的页面错误
  if (cause == 13 && vf->readable == 0)
    return -1;
  // 写导致的页面错误
  if (cause == 15 && vf->writable == 0)
    return -1;

  void *pa = kalloc();
  if (pa == 0)
    return -1;
  memset(pa, 0, PGSIZE);

  // 读取文件内容
  ilock(vf->ip);
  // 计算当前页面读取文件的偏移量，实验中p->vma[i].offset总是0
  // 要按顺序读读取，例如内存页面A,B和文件块a,b
  // 则A读取a，B读取b，而不能A读取b，B读取a
  int offset = p->vma[i].offset + PGROUNDDOWN(va - p->vma[i].addr);
  int readbytes = readi(vf->ip, 0, (uint64)pa, offset, PGSIZE);
  // 如果读到0字节，可能是文件末尾，这是正常情况
  // 不应该返回错误，而是继续使用已分配的页面（已清零）
  if (readbytes == 0)
  {
    iunlock(vf->ip);
    // 继续使用已分配的页面（已清零）
  }
  iunlock(vf->ip);

  // 添加页面映射
  if (mappages(p->pagetable, PGROUNDDOWN(va), PGSIZE, (uint64)pa, pte_flags) != 0)
  {
    kfree(pa);
    return -1;
  }

  return 0;
}