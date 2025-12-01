#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "time.h"

volatile static int started = 0;

void
printlogo()
{
  printf("\n");
  printf("███████╗ ██╗  ██╗ ██╗  ██╗  ██████╗  ███████╗\n");
  printf("╚══███╔╝ ╚██╗██╔╝ ╚██╗██╔╝ ██╔═══██╗ ██╔════╝\n");
  printf("  ███╔╝   ╚███╔╝   ╚███╔╝  ██║   ██║ ███████╗\n");
  printf(" ███╔╝    ██╔██╗   ██╔██╗  ██║   ██║ ╚════██║\n");
  printf("███████╗ ██╔╝ ██╗ ██╔╝ ██╗ ╚██████╔╝ ███████║\n");
  printf("╚══════╝ ╚═╝  ╚═╝ ╚═╝  ╚═╝  ╚═════╝  ╚══════╝\n");
  printf("\n");
}

static void
printdate()
{
  struct timeval tv;
  struct tm tm;
  gettimeofday(&tv, NULL);
  localtime_r(&tv.tv_sec, &tm);
  printf("%04d/%02d/%02d %02d:%02d:%02d\n",
    tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
}

// start() jumps here in supervisor mode on all CPUs.
void
main()
{
  if(cpuid() == 0){
    consoleinit();
// LAB_LOCK
    statsinit();
// END LAB_LOCK
    printfinit();
    printlogo();
    printf("\n");
    printf("ZXXOS kernel is booting\n");
    printf("\n");
    kinit();         // physical page allocator
    kvminit();       // create kernel page table
    kvminithart();   // turn on paging
    procinit();      // process table
    trapinit();      // trap vectors
    trapinithart();  // install kernel trap vector
    plicinit();      // set up interrupt controller
    plicinithart();  // ask PLIC for device interrupts
    binit();         // buffer cache
    iinit();         // inode table
    fileinit();      // file table
    shm_init();       // shared memory
    sem_init();       // semaphore
    msg_init();       // message queue
    virtio_disk_init(); // emulated hard disk
    printdate();
    // LAB_NET
    pci_init();
    netinit();
    // END LAB_NET
    userinit();      // first user process
    __sync_synchronize();
    started = 1;
  } else {
    while(atomic_read4((int *) &started) == 0)
      ;
    __sync_synchronize();
    printf("hart %d starting\n", cpuid());
    kvminithart();    // turn on paging
    trapinithart();   // install kernel trap vector
    plicinithart();   // ask PLIC for device interrupts
  }

  scheduler();        
}
