#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/pwd.h"
#include "kernel/fcntl.h"

#define MAXLEN 30

// Global variables
int fd_passwd = 0;

// Reads one line from buf at position i into field
int
getpwfield(char *field, char *buf, int i)
{
  int c = 0;
  while(buf[i] != ':' && buf[i] != '\n'){
    field[c++] = buf[i++];
  }
  field[c] = '\0';
  return ++i;
}

// Returns null on error
struct passwd *
getpwent(void)
{
  if(!fd_passwd)
    setpwent(); // open passwd file if fd_passwd not set

  // TODO: implement `getpwent`

  // general procedure would be to:
  //    1. get fields from passwd file entry

  struct passwd *p = malloc(sizeof(struct passwd));
  //    2. write fields into `struct passwd`

  return p;
}

void 
setpwent(void)
{
  if((fd_passwd = open(PASSWD_PATH, O_RDONLY)) < 0){
    // void return type means failure doesn't matter, I guess
    // printf("Unable to open file: %s, errno :%d\n", PASSWD_PATH, fd_passwd); 
  }
}

void 
endpwent(void)
{
  close(fd_passwd);
  fd_passwd = 0;
}

struct passwd *
getpwnam(const char *name)
{
  // TODO: implement `getpwnam`
  return NULL;
}

struct passwd *
getpwuid(uint uid)
{
  // TODO: implement `getpwuid`
  return NULL;
}

// Write given passwd entry into passwd file
int 
putpwent(const struct passwd *p)
{
  // TODO: implement `putpwent`
  // tip: can use `fprintf` to write to file
  return 0;
}