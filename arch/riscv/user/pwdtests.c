#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/pwd.h"
#include "user/hash.h"
#include "kernel/fcntl.h"

void
test_putpwent(char *name, char *password, uint uid, uint gid, char *gecos, char *dir, char *shell)
{
  unsigned int rand = hash_rand();
//   unsigned int hash = jenkins_one_at_a_time_hash(password, strlen(password));
  char hashed_password[32];
  generate_hashed_password_with_salt(password, rand, hashed_password);
  
  const struct passwd p = {
    .pw_name = name,
    .pw_passwd = hashed_password,
    .pw_uid = uid,
    .pw_gid = gid,
    .pw_gecos = gecos,
    .pw_dir = dir,
    .pw_shell = shell
  };
  putpwent(&p);
}

void
test_getpwent(void)
{
  struct passwd *p = getpwent();
  if (p) {
    printf("%s:%s:%d:%d:%s:%s:%s\n",
        p->pw_name,
        p->pw_passwd,
        p->pw_uid,
        p->pw_gid,
        p->pw_gecos,  // user information
        p->pw_dir,    // home dir
        p->pw_shell   // shell program
        );
    // 不需要释放p，因为它指向静态变量
  }
}

void
test_getpwuid(uint uid)
{
  struct passwd *p = getpwuid(uid);
  if (p) {
    printf("%s:%s:%d:%d:%s:%s:%s\n",
        p->pw_name,
        p->pw_passwd,
        p->pw_uid,
        p->pw_gid,
        p->pw_gecos,  // user information
        p->pw_dir,    // home dir
        p->pw_shell   // shell program
        );
    // 不需要释放p，因为它指向静态变量
  } else {
    printf("entry with uid %d not found\n", uid);
  }
}

void
test_getpwnam(char *nam)
{
  struct passwd *p = getpwnam(nam);
  if (p) {
    printf("%s:%s:%d:%d:%s:%s:%s\n",
        p->pw_name,
        p->pw_passwd,
        p->pw_uid,
        p->pw_gid,
        p->pw_gecos,  // user information
        p->pw_dir,    // home dir
        p->pw_shell   // shell program
        );
    // 不需要释放p，因为它指向静态变量
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