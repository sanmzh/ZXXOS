//
// Support functions for system calls that involve file descriptors.
//

#include "types.h"
#ifdef loongarch
#include "loongarch.h"
#endif
#ifdef riscv
#include "riscv.h"
#endif
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "stat.h"
#include "proc.h"

struct devsw devsw[NDEV];
struct {
  struct spinlock lock;
  struct file file[NFILE];
} ftable;

void
fileinit(void)
{
  initlock(&ftable.lock, "ftable");
}

// Allocate a file structure.
struct file*
filealloc(void)
{
  struct file *f;

  acquire(&ftable.lock);
  for(f = ftable.file; f < ftable.file + NFILE; f++){
    if(f->ref == 0){
      f->ref = 1;
      release(&ftable.lock);
      return f;
    }
  }
  release(&ftable.lock);
  return 0;
}

// Increment ref count for file f.
struct file*
filedup(struct file *f)
{
  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("filedup");
  f->ref++;
  release(&ftable.lock);
  return f;
}

// Close file f.  (Decrement ref count, close when reaches 0.)
void
fileclose(struct file *f)
{
  struct file ff;

  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("fileclose");
  if(--f->ref > 0){
    release(&ftable.lock);
    return;
  }
  ff = *f;
  f->ref = 0;
  f->type = FD_NONE;
  release(&ftable.lock);

  if(ff.type == FD_PIPE){
    pipeclose(ff.pipe, ff.writable);
  } else if(ff.type == FD_INODE || ff.type == FD_DEVICE){
    begin_op();
    iput(ff.ip);
    end_op();
  }
#ifdef riscv
  else if(ff.type == FD_SOCKET){
    socket_close(ff.socket);
  }
#endif
}

// Get metadata about file f.
// addr is a user virtual address, pointing to a struct stat.
int
filestat(struct file *f, uint64 addr)
{
  struct proc *p = myproc();
  struct stat st;
  
  if(f->type == FD_INODE || f->type == FD_DEVICE){
    ilock(f->ip);
    stati(f->ip, &st);
    iunlock(f->ip);
    if(copyout(p->pagetable, addr, (char *)&st, sizeof(st)) < 0)
      return -1;
    return 0;
  }
  return -1;
}

// Read from file f.
// addr is a user virtual address.
int
fileread(struct file *f, uint64 addr, int n)
{
  int r = 0;

  if(f->readable == 0)
    return -1;

  if(f->type == FD_PIPE){
    r = piperead(f->pipe, addr, n);
  } else if(f->type == FD_DEVICE){
    if(f->major < 0 || f->major >= NDEV || !devsw[f->major].read)
      return -1;
    r = devsw[f->major].read(1, addr, n);
  } else if(f->type == FD_INODE){
    // 检查文件读取权限
    ilock(f->ip);
    #ifdef riscv
    // 检查当前进程是否有读取权限
    struct proc *p = myproc();
    uint uid = p->uid;
    uint gid = p->gid;
    uint fmode = f->ip->mode;
    uint fuid = f->ip->uid;
    uint fgid = f->ip->gid;
    
    // root用户拥有所有权限
    if(uid != 0) {
      // 检查读权限
      if(!((fmode & 1<<8 && uid == fuid) ||    // 所有者读权限 (0400)
         (fmode & 1<<5 && gid == fgid) ||    // 组读权限 (0040)
         (fmode & 1<<2))) {                   // 其他用户读权限 (0004)
        iunlock(f->ip);
        return -1;
      }
    }
    #endif
    
    if((r = readi(f->ip, 1, addr, f->off, n)) > 0)
      f->off += r;
    iunlock(f->ip);
  } else {
    panic("fileread");
  }

  return r;
}

// Write to file f.
// addr is a user virtual address.
int
filewrite(struct file *f, uint64 addr, int n)
{
  int r, ret = 0;

  if(f->writable == 0)
    return -1;

  if(f->type == FD_PIPE){
    ret = pipewrite(f->pipe, addr, n);
  } else if(f->type == FD_DEVICE){
    if(f->major < 0 || f->major >= NDEV || !devsw[f->major].write)
      return -1;
    ret = devsw[f->major].write(1, addr, n);
  } else if(f->type == FD_INODE){
    #ifdef riscv
    // 检查文件写入权限
    ilock(f->ip);
    // 检查当前进程是否有写入权限
    struct proc *p = myproc();
    uint uid = p->uid;
    uint gid = p->gid;
    uint fmode = f->ip->mode;
    uint fuid = f->ip->uid;
    uint fgid = f->ip->gid;
    
    // root用户拥有所有权限
    if(uid != 0) {
      // 检查写权限
      if(!((fmode & 1<<7 && uid == fuid) ||    // 所有者写权限 (0200)
         (fmode & 1<<4 && gid == fgid) ||    // 组写权限 (0020)
         (fmode & 1<<1))) {                   // 其他用户写权限 (0002)
        iunlock(f->ip);
        return -1;
      }
    }
    iunlock(f->ip); // 权限检查完成后释放锁
    #endif
    
    // write a few blocks at a time to avoid exceeding
    // maximum log transaction size, including
    // i-node, indirect block, allocation blocks,
    // and 2 blocks of slop for non-aligned writes.
    int max = ((MAXOPBLOCKS-1-1-2) / 2) * BSIZE;
    int i = 0;
    while(i < n){
      int n1 = n - i;
      if(n1 > max)
        n1 = max;

      begin_op();
      ilock(f->ip);
      #ifdef riscv
      // 再次检查权限，确保在持有锁期间权限没有被修改
      if(uid != 0) {
        if(!((f->ip->mode & 1<<7 && uid == f->ip->uid) ||    // 所有者写权限 (0200)
           (f->ip->mode & 1<<4 && gid == f->ip->gid) ||    // 组写权限 (0020)
           (f->ip->mode & 1<<1))) {                         // 其他用户写权限 (0002)
          iunlock(f->ip);
          end_op();
          return -1;
        }
      }
      #endif
      
      if ((r = writei(f->ip, 1, addr + i, f->off, n1)) > 0)
        f->off += r;
      iunlock(f->ip);
      end_op();

      if(r != n1){
        // error from writei
        break;
      }
      i += r;
    }
    ret = (i == n ? n : -1);
  } else {
    panic("filewrite");
  }

  return ret;
}


// TODO!!!: write kernel functions for `chown`, `chmod`

