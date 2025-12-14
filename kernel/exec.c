#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#ifdef riscv
#define my_exec kexec 
#include "riscv.h"

#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#endif
#ifdef loongarch
#include "loongarch.h"
#define my_exec exec 
#endif
#include "proc.h"
#include "defs.h"
#include "elf.h"

// ASLR random seed declaration
extern uint64 g_random_seed;

// Generate a random number between min and max using g_random_seed
int get_random_min_max(int min, int max) 
{
  if (min > max) {
    int temp = min;
    min = max;
    max = temp;
  }
  
  // Update the random seed using a simple linear congruential generator
  g_random_seed = g_random_seed * 1103515245 + 12345;
  
  // Generate a random number in the specified range
  int randomNum = min + (g_random_seed % (max - min + 1));
  
  return randomNum;
}
#ifdef riscv
static int loadseg(pde_t *, uint64, struct inode *, uint, uint);
// map ELF permissions to PTE permission bits.
int flags2perm(int flags)
{
    int perm = 0;
    if(flags & 0x1)
      perm = PTE_X;
    if(flags & 0x2)
      perm |= PTE_W;
    return perm;
}
#endif
#ifdef loongarch
static int loadseg(pde_t *pgdir, uint64 addr, struct inode *ip, uint offset, uint sz);
#endif
//
// the implementation of the exec() system call
//
int
my_exec(char *path, char **argv)
{
  char *s, *last;
  int i, off;
  uint64 argc, sz = 0, sp, ustack[MAXARG], stackbase;
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pagetable_t pagetable = 0, oldpagetable;
  struct proc *p = myproc();
  
  // ASLR flag - set to 1 to enable ASLR, 0 to disable
  // Only enable ASLR for RISC-V architecture
  #ifdef riscv
  int aslr_enabled = 1;  // Re-enable ASLR with heap randomization only
  #else
  int aslr_enabled = 0;
  #endif

  begin_op();

  // Open the executable file.
  if((ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);

  // Read the ELF header.
  if(readi(ip, 0, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;

  // Is this really an ELF file?
  if(elf.magic != ELF_MAGIC)
    goto bad;

  if((pagetable = proc_pagetable(p)) == 0)
    goto bad;

  // ASLR: Calculate base offset for code segment
  // Disable code segment randomization to avoid breaking relative addressing
  int base_pointer_offset = 0;  // aslr_enabled ? get_random_min_max(0, 1) : 0;
  uint64 text_seg_pages_offset = base_pointer_offset * PGSIZE;

  // Load program into memory.
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    uint64 sz1;
    #ifdef riscv
    if(ph.vaddr % PGSIZE != 0)
      goto bad;
    if((sz1 = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz + text_seg_pages_offset, flags2perm(ph.flags))) == 0)
      goto bad;
    #endif
    #ifdef loongarch
    if((sz1 = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz)) == 0)
      goto bad;
    if((ph.vaddr % PGSIZE) != 0)
      goto bad;
    #endif
    sz = sz1;
    #ifdef riscv
    if(loadseg(pagetable, ph.vaddr + text_seg_pages_offset, ip, ph.off, ph.filesz) < 0)
      goto bad;
    #else
    if(loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
    #endif
  }
  // TODO!!!: set program set-user-ID capability


  iunlockput(ip);
  end_op();
  ip = 0;

  p = myproc();
  uint64 oldsz = p->sz;
  
  #ifdef riscv
  // 保存进程的UID和GID，以便在新程序中保留
  uint old_uid = p->uid;
  uint old_gid = p->gid;
  #endif

  // Allocate some pages at the next page boundary.
  // Make the first inaccessible as a stack guard.
  // Use the rest as the user stack.
  sz = PGROUNDUP(sz);
  uint64 sz1;
  
  // ASLR: Randomize heap size
  int num_pages = 2;  // Base number of pages
  if (aslr_enabled) {
    int add_pages = get_random_min_max(0, 8);
    num_pages += add_pages;
  }
  
  #ifdef riscv
  if((sz1 = uvmalloc(pagetable, sz, sz + (USERSTACK+num_pages)*PGSIZE, PTE_W)) == 0)
    goto bad;
  sz = sz1;
  uvmclear(pagetable, sz-(USERSTACK+num_pages)*PGSIZE);
  sp = sz;
  
  // ASLR: Randomize stack offset
  // Temporarily disable stack randomization to test heap randomization
  // if (aslr_enabled) {
  //   int stack_offset = get_random_min_max(0, 16);
  //   sp -= stack_offset * 64;  // Adjust stack pointer by random offset
  //   sp -= num_pages * PGSIZE;  // Adjust for additional pages
  // }
  
  stackbase = sp - USERSTACK*PGSIZE;
  #endif
  #ifdef loongarch
  if((sz1 = uvmalloc(pagetable, sz, sz + 2*PGSIZE)) == 0)
    goto bad;
  sz = sz1;
  uvmclear(pagetable, sz-2*PGSIZE);
  sp = sz;
  
  // ASLR: Randomize stack offset
  // Temporarily disable stack randomization to test heap randomization
  // if (aslr_enabled) {
  //   int stack_offset = get_random_min_max(0, 16);
  //   sp -= stack_offset * 64;  // Adjust stack pointer by random offset
  // }
  
  stackbase = sp - PGSIZE;
  #endif

  // Copy argument strings into new stack, remember their
  // addresses in ustack[].
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp -= strlen(argv[argc]) + 1;
    sp -= sp % 16; // sp must be 16-byte aligned in two arch
    if(sp < stackbase)
      goto bad;
    if(copyout(pagetable, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[argc] = sp;
  }
  ustack[argc] = 0;

  // push a copy of ustack[], the array of argv[] pointers.
  sp -= (argc+1) * sizeof(uint64);
  sp -= sp % 16;
  if(sp < stackbase)
    goto bad;
  if(copyout(pagetable, sp, (char *)ustack, (argc+1)*sizeof(uint64)) < 0)
    goto bad;

  // a0 and a1 contain arguments to user main(argc, argv)
  // argc is returned via the system call return
  // value, which goes in a0.
  p->trapframe->a1 = sp;

  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(p->name, last, sizeof(p->name));
    
  // Commit to the user image.
  oldpagetable = p->pagetable;
  p->pagetable = pagetable;
  p->sz = sz;
  #ifdef riscv
  p->trapframe->epc = elf.entry + text_seg_pages_offset;  // initial program counter = ulib.c:start()
  #endif
  #ifdef loongarch
  p->trapframe->era = elf.entry;  // initial program counter = ulib.c:start()
  #endif
  p->trapframe->sp = sp; // initial stack pointer
  proc_freepagetable(oldpagetable, oldsz);
  
  #ifdef riscv
  // 恢复进程的UID和GID
  p->uid = old_uid;
  p->gid = old_gid;
  #endif

  return argc; // this ends up in a0, the first argument to main(argc, argv)

 bad:
  if(pagetable)
    proc_freepagetable(pagetable, sz);
  if(ip){
    iunlockput(ip);
    end_op();
  }
  return -1;
}

// Load an ELF program segment into pagetable at virtual address va.
// va must be page-aligned
// and the pages from va to va+sz must already be mapped.
// Returns 0 on success, -1 on failure.
static int
loadseg(pagetable_t pagetable, uint64 va, struct inode *ip, uint offset, uint sz)
{
  uint i, n;
  uint64 pa;

  for(i = 0; i < sz; i += PGSIZE){
    pa = walkaddr(pagetable, va + i);
    if(pa == 0)
      panic("loadseg: address should exist");
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    #ifdef riscv
    if(readi(ip, 0, (uint64)pa, offset+i, n) != n)
      return -1;
    #endif
    #ifdef loongarch
    if(readi(ip, 0, (uint64)(pa | DMWIN_MASK), offset+i, n) != n)
      return -1;
    #endif
  }
  
  return 0;
}
