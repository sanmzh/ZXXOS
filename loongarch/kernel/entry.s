# 0 "loongarch/kernel/entry.S"
# 0 "<built-in>"
# 0 "<命令行>"
# 1 "/opt/cross-tools/target/usr/include/stdc-predef.h" 1 3 4
# 0 "<命令行>" 2
# 1 "loongarch/kernel/entry.S"
 # qemu -kernel loads the kernel at 0x00000000
        # and causes each CPU to jump there.
        # kernel.ld causes the following code to
        # be placed at 0x00000000.

# 1 "loongarch/kernel/asm.h" 1
# 7 "loongarch/kernel/entry.S" 2

.section .text
.global _entry
_entry:
        la.global $t0, 0f
        jirl $zero, $t0, 0
0: li.d $t0, 0x9000000000000011 # CA, PLV0, 0x9000 xxxx xxxx xxxx
        csrwr $t0, 0x180
        li.d $t0, 0x0000000000000000
        csrwr $t0, 0x181
        csrwr $t0, 0x182
        csrwr $t0, 0x183
        csrwr $t0, 0x88
        li.w $t0, 0xb0 # PLV=0, IE=0, PG=1
 csrwr $t0, 0x0
        invtlb 0x0,$zero,$zero
 # set up a stack for C.
        # stack0 is declared in start.c,
        # with a 4096-byte stack per CPU.
        # sp = stack0 + (hartid * 4096)

        la $sp, stack0
        li.d $a0, 1024*4
 csrrd $a1, 0x20
        addi.d $tp, $a1, 0
        addi.d $a1, $a1, 1
        mul.d $a0, $a0, $a1
        add.d $sp, $sp, $a0
 # jump to start() in start.c
        bl main
spin:
        b spin
