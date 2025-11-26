#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#include <stdarg.h>

static char digits[] = "0123456789ABCDEF";

static void
putc(int fd, char c)
{
  write(fd, &c, 1);
}

static void
printint(int fd, long long xx, int base, int sgn)
{
  char buf[20];
  int i, neg;
  unsigned long long x;

  neg = 0;
  if(sgn && xx < 0){
    neg = 1;
    x = -xx;
  } else {
    x = xx;
  }

  i = 0;
  do{
    buf[i++] = digits[x % base];
  }while((x /= base) != 0);
  if(neg)
    buf[i++] = '-';

  while(--i >= 0)
    putc(fd, buf[i]);
}

static void
printptr(int fd, uint64 x) {
  int i;
  putc(fd, '0');
  putc(fd, 'x');
  for (i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4)
    putc(fd, digits[x >> (sizeof(uint64) * 8 - 4)]);
}

// Print to the given fd. Only understands %d, %x, %p, %c, %s.
void
vprintf(int fd, const char *fmt, va_list ap)
{
  char *s;
  int c0, c1, c2, i, state;

  state = 0;
  for(i = 0; fmt[i]; i++){
    c0 = fmt[i] & 0xff;
    if(state == 0){
      if(c0 == '%'){
        state = '%';
      } else {
        putc(fd, c0);
      }
    } else if(state == '%'){
      c1 = c2 = 0;
      if(c0) c1 = fmt[i+1] & 0xff;
      if(c1) c2 = fmt[i+2] & 0xff;
      if(c0 == 'd'){
        printint(fd, va_arg(ap, int), 10, 1);
      } else if(c0 == 'l' && c1 == 'd'){
        printint(fd, va_arg(ap, uint64), 10, 1);
        i += 1;
      } else if(c0 == 'l' && c1 == 'l' && c2 == 'd'){
        printint(fd, va_arg(ap, uint64), 10, 1);
        i += 2;
      } else if(c0 == 'u'){
        printint(fd, va_arg(ap, uint32), 10, 0);
      } else if(c0 == 'l' && c1 == 'u'){
        printint(fd, va_arg(ap, uint64), 10, 0);
        i += 1;
      } else if(c0 == 'l' && c1 == 'l' && c2 == 'u'){
        printint(fd, va_arg(ap, uint64), 10, 0);
        i += 2;
      } else if(c0 == 'x'){
        printint(fd, va_arg(ap, uint32), 16, 0);
      } else if(c0 == 'l' && c1 == 'x'){
        printint(fd, va_arg(ap, uint64), 16, 0);
        i += 1;
      } else if(c0 == 'l' && c1 == 'l' && c2 == 'x'){
        printint(fd, va_arg(ap, uint64), 16, 0);
        i += 2;
      } else if(c0 == 'p'){
        printptr(fd, va_arg(ap, uint64));
      } else if(c0 == 'c'){
        putc(fd, va_arg(ap, uint32));
      } else if(c0 == 's'){
        if((s = va_arg(ap, char*)) == 0)
          s = "(null)";
        for(; *s; s++)
          putc(fd, *s);
      } else if(c0 == '%'){
        putc(fd, '%');
      } else {
        // Unknown % sequence.  Print it to draw attention.
        putc(fd, '%');
        putc(fd, c0);
      }

      state = 0;
    }
  }
}

void
fprintf(int fd, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vprintf(fd, fmt, ap);
}

void
printf(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vprintf(1, fmt, ap);
}

// 辅助函数：将整数写入字符串缓冲区
static void
sprintint(char **buf, long long xx, int base, int sgn)
{
  char tmp[20];
  int i, neg;
  unsigned long long x;

  neg = 0;
  if(sgn && xx < 0){
    neg = 1;
    x = -xx;
  } else {
    x = xx;
  }

  i = 0;
  do{
    tmp[i++] = digits[x % base];
  }while((x /= base) != 0);
  if(neg)
    tmp[i++] = '-';

  while(--i >= 0)
    *(*buf)++ = tmp[i];
}

// 辅助函数：将指针写入字符串缓冲区
static void
sprintptr(char **buf, uint64 x) {
  int i;
  *(*buf)++ = '0';
  *(*buf)++ = 'x';
  for (i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4)
    *(*buf)++ = digits[x >> (sizeof(uint64) * 8 - 4)];
}


// Print to a string buffer. Only understands %d, %x, %p, %c, %s.
int
vsprintf(char *buf, const char *fmt, va_list ap)
{
  char *s;
  int c0, c1, c2, i, state;
  char *orig_buf = buf;  // 保存原始缓冲区指针，用于返回写入的字符数

  state = 0;
  for(i = 0; fmt[i]; i++){
    c0 = fmt[i] & 0xff;
    if(state == 0){
      if(c0 == '%'){
        state = '%';
      } else {
        *buf++ = c0;
      }
    } else if(state == '%'){
      c1 = c2 = 0;
      if(c0) c1 = fmt[i+1] & 0xff;
      if(c1) c2 = fmt[i+2] & 0xff;
      if(c0 == 'd'){
        sprintint(&buf, va_arg(ap, int), 10, 1);
      } else if(c0 == 'l' && c1 == 'd'){
        sprintint(&buf, va_arg(ap, uint64), 10, 1);
        i += 1;
      } else if(c0 == 'l' && c1 == 'l' && c2 == 'd'){
        sprintint(&buf, va_arg(ap, uint64), 10, 1);
        i += 2;
      } else if(c0 == 'u'){
        sprintint(&buf, va_arg(ap, uint32), 10, 0);
      } else if(c0 == 'l' && c1 == 'u'){
        sprintint(&buf, va_arg(ap, uint64), 10, 0);
        i += 1;
      } else if(c0 == 'l' && c1 == 'l' && c2 == 'u'){
        sprintint(&buf, va_arg(ap, uint64), 10, 0);
        i += 2;
      } else if(c0 == 'x'){
        sprintint(&buf, va_arg(ap, uint32), 16, 0);
      } else if(c0 == 'l' && c1 == 'x'){
        sprintint(&buf, va_arg(ap, uint64), 16, 0);
        i += 1;
      } else if(c0 == 'l' && c1 == 'l' && c2 == 'x'){
        sprintint(&buf, va_arg(ap, uint64), 16, 0);
        i += 2;
      } else if(c0 == 'p'){
        sprintptr(&buf, va_arg(ap, uint64));
      } else if(c0 == 'c'){
        *buf++ = va_arg(ap, uint32);
      } else if(c0 == 's'){
        if((s = va_arg(ap, char*)) == 0)
          s = "(null)";
        for(; *s; s++)
          *buf++ = *s;
      } else if(c0 == '%'){
        *buf++ = '%';
      } else {
        // Unknown % sequence. Print it to draw attention.
        *buf++ = '%';
        *buf++ = c0;
      }

      state = 0;
    }
  }

  *buf = '\0';  // 添加字符串结束符
  return buf - orig_buf;  // 返回写入的字符数
}

int
sprintf(char *buf, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  return vsprintf(buf, fmt, ap);
}

