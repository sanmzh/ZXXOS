#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/pwd.h"
#include "kernel/fcntl.h"

void
test_putpwent(char *name, char *password, uint uid, uint gid, char *gecos, char *dir, char *shell)
{
  uint rand = get_random();
  uint hash = get_hash((uchar*)password, strlen(password), rand);
  const struct passwd p = {
    name,
    {hash, rand},
    uid,
    gid,
    gecos,
    dir,
    shell
  };
  putpwent(&p);
}

void
test_getpwent(void)
{
  struct passwd *p = getpwent();
  if (p) {
    printf("%s:%d$%d:%d:%d:%s:%s:%s\n",
        p->name,
        p->p_passwd.hash,
        p->p_passwd.rand,
        p->uid,
        p->gid,
        p->gecos,  // user information
        p->dir,    // home dir
        p->shell   // shell program
        );
    free(p);
  }
}

void
test_getpwuid(uint uid)
{
  struct passwd *p = getpwuid(uid);
  if (p) {
    printf("%s:%d$%d:%d:%d:%s:%s:%s\n",
        p->name,
        p->p_passwd.hash,
        p->p_passwd.rand,
        p->uid,
        p->gid,
        p->gecos,  // user information
        p->dir,    // home dir
        p->shell   // shell program
        );
    free(p);
  } else {
    printf("entry with uid %d not found\n", uid);
  }
}

void
test_getpwnam(char *nam)
{
  struct passwd *p = getpwnam(nam);
  if (p) {
    printf("%s:%d$%d:%d:%d:%s:%s:%s\n",
        p->name,
        p->p_passwd.hash,
        p->p_passwd.rand,
        p->uid,
        p->gid,
        p->gecos,  // user information
        p->dir,    // home dir
        p->shell   // shell program
        );
    free(p);
  } else {
    printf("entry with name %s not found\n", nam);
  }
}

void
test_setpwent(void)
{
  setpwent();
}

void
test_endpwent(void)
{
  endpwent();
}

int
main(int argc, char *argv[])
{
  test_putpwent("username", "password", 1000, 1001, "", "/home/username", "");
  test_putpwent("root",     "root",     0,    0,    "", "/home/root",     "");
  test_putpwent("john",     "abcd",     1005, 1006, "", "/home/john",     "");

  printf("\n");

  test_getpwent();
  test_getpwent();
  test_getpwent();

  printf("\n");

  test_setpwent();
  test_getpwent();
  test_setpwent();
  test_getpwent();

  printf("\n");

  test_setpwent();
  test_getpwent();
  test_getpwent();
  test_getpwent();
  test_getpwent(); // should loop back to beginning of file

  printf("\n");

  test_setpwent();
  test_getpwuid(0);
  test_getpwuid(10);
  test_setpwent();
  test_getpwnam("john");
  test_getpwnam("mary");

  printf("\n");

  test_setpwent();
  test_endpwent();
  test_getpwent();

  exit(0);
}