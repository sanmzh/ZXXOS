#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/pwd.h"
#include "kernel/fcntl.h"

#define MAXLEN 128 
#define PASSWORD_LEN 8

uint
create_uid(void)
{
  struct passwd *p;
  int uid = 0; // first user created will be root (uid 0)
  while((p = getpwuid(uid))){
    free(p);
    uid++;
  }
  return uid;
}

int
main(int argc, char *argv[])
{
  // TODO: prompt for username

  // TODO: check if user already exists

  // TODO: prompt for password

  // TODO: hash and salt password

  // TODO; create user account entry
}