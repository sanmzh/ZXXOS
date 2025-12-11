#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/pwd.h"
#include "kernel/fcntl.h"

#define MAX_ATTEMPTS 3
#define MAXLEN 30

int
main(int argc, char *argv[])
{
  // TODO: check if pwd file exists, if not, create root user

  // TODO: prompt for username

  // TODO: prompt for password

  // TODO: authenticate user given input
  // TODO: upon successful authentication: set process UID, GID to identity
  // TODO: upon successful authentication: execute shell
}