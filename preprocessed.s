# 0 "arch/loongarch/user/initcode.S"
# 0 "<built-in>"
# 0 "<command-line>"
# 1 "/usr/include/stdc-predef.h" 1 3 4
# 0 "<command-line>" 2
# 1 "arch/loongarch/user/initcode.S"
# Initial process that execs /init.
# This code runs in user space.

# 1 "arch/loongarch/kernel/syscall.h" 1
# 5 "arch/loongarch/user/initcode.S" 2

# exec(init, argv)
.globl start
start:
        la $a0, init
        la $a1, argv
        li.d $a7, 221
        syscall 0

# for(;;) exit();
exit:
        li.d $a7, 93
        syscall 0
        bl exit

# char init[] = "/init\0";
init:
  .string "/init\0"

# char *argv[] = { init, 0 };
.p2align 2
argv:
  .long init
  .long 0
