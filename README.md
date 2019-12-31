# jitmap: Jitted bitmaps

## Language

jitmap offers a small DSL language to evaluate bitwise operations on bitmaps.
The language supports variables (named bitmap), empty/full literals, and basic
operators (not, and, or xor).

A query takes an expression and a list of bitmaps and excute the expression on
the bitmaps resulting in an new bitmap.

### Supported expressions

 - Empty bitmap literal: `$0`
 - Full bitmap literal: `$1`
 - Variables (named bitmap): `[A-Za-z0-9_]+`, e.g. `country`, `color_red`
 - Not: `!e`
 - And: `e_1 & e_2`
 - Or: `e_1 | e_2`
 - Xor: `e_1 ^ e_2`

### Examples
```
# NOT(a)
!a

# a AND b
a & b

# $empty AND (a OR b) XOR c
($0 & (a | b) ^ c)
```

## Developing/Debugging

### *jitmap-ir* tool

The *jitmap-ir* tool takes an expression as first input argument and dumps the
generated LLVM ir to stdout. By default, this will not use vectorized instruction.

```llvm
# tools/jitmap-ir '(a & b & c & d | e ^ f)'
; ModuleID = 'jitmap-ir-module'
source_filename = "jitmap-ir-module"

; Function Attrs: argmemonly
define void @query(i64* nocapture readonly %in, i64* nocapture readonly %in1, i64* nocapture readonly %in2, i64* nocapture readonly %in3, i64* nocapture readonly %in4, i64* nocapture readonly %in5, i64* nocapture %out) #0 {
entry:
  br label %loop

loop:                                             ; preds = %loop, %entry
  %i = phi i64 [ 0, %entry ], [ %next_i, %loop ]
  %gep_0 = getelementptr inbounds i64, i64* %in, i64 %i
  %load_0 = load i64, i64* %gep_0
  %gep_1 = getelementptr inbounds i64, i64* %in1, i64 %i
  %load_1 = load i64, i64* %gep_1
  %gep_2 = getelementptr inbounds i64, i64* %in2, i64 %i
  %load_2 = load i64, i64* %gep_2
  %gep_3 = getelementptr inbounds i64, i64* %in3, i64 %i
  %load_3 = load i64, i64* %gep_3
  %gep_4 = getelementptr inbounds i64, i64* %in4, i64 %i
  %load_4 = load i64, i64* %gep_4
  %gep_5 = getelementptr inbounds i64, i64* %in5, i64 %i
  %load_5 = load i64, i64* %gep_5
  %0 = and i64 %load_0, %load_1
  %1 = and i64 %0, %load_2
  %2 = and i64 %1, %load_3
  %3 = xor i64 %load_4, %load_5
  %4 = or i64 %2, %3
  %gep_output = getelementptr inbounds i64, i64* %out, i64 %i
  store i64 %4, i64* %gep_output
  %next_i = add i64 %i, 1
  %exit_cond = icmp eq i64 %next_i, 1024
  br i1 %exit_cond, label %after_loop, label %loop

after_loop:                                       ; preds = %loop
  ret void
}

attributes #0 = { argmemonly }
```

We can then use LLVM's `llc` to transform the IR into native assembly.

```objdump
# tools/jitmap-ir '(a & b & c & d | e ^ f)' | llc-8 -O3 -mcpu=core-avx2
ninja: no work to do.
        .text
        .file   "jitmap-ir-module"
        .globl  query                   # -- Begin function query
        .p2align        4, 0x90
        .type   query,@function
query:                                  # @query
        .cfi_startproc
# %bb.0:                                # %entry
        pushq   %rbx
        .cfi_def_cfa_offset 16
        .cfi_offset %rbx, -16
        movq    16(%rsp), %r10
        xorl    %eax, %eax
        .p2align        4, 0x90
.LBB0_1:                                # %loop
                                        # =>This Inner Loop Header: Depth=1
        movq    (%rdi,%rax), %r11
        movq    (%r8,%rax), %rbx
        andq    (%rsi,%rax), %r11
        andq    (%rdx,%rax), %r11
        andq    (%rcx,%rax), %r11
        xorq    (%r9,%rax), %rbx
        orq     %r11, %rbx
        movq    %rbx, (%r10,%rax)
        addq    $8, %rax
        cmpq    $8192, %rax             # imm = 0x2000
        jne     .LBB0_1
# %bb.2:                                # %after_loop
        popq    %rbx
        .cfi_def_cfa_offset 8
        retq
.Lfunc_end0:
        .size   query, .Lfunc_end0-query
        .cfi_endproc
                                        # -- End function

        .section        ".note.GNU-stack","",@progbits

```

This code is still not fully optimized, `opt` is used for this.

```assembly
# tools/jitmap-ir '(a & b & c & d | e ^ f)' | opt-8 -S -O3 -mcpu=core-avx2 -mtriple=x86_64-unknown-linux-gnu | llc-8 -O3 -mcpu=core-avx2
        .text
        .file   "jitmap-ir-module"
        .section        .rodata.cst8,"aM",@progbits,8
        .p2align        3               # -- Begin function query
.LCPI0_0:
        .quad   8192                    # 0x2000
.LCPI0_1:
        .quad   -9223372036854775808    # 0x8000000000000000
        .text
        .globl  query
        .p2align        4, 0x90
        .type   query,@function
query:                                  # @query
# %bb.0:                                # %entry
        pushq   %rbp
        pushq   %r14
        pushq   %rbx
        movq    32(%rsp), %r10
        leaq    8192(%r10), %rbp
        vmovq   %rcx, %xmm0
        vmovq   %rdx, %xmm1
        vpunpcklqdq     %xmm0, %xmm1, %xmm0 # xmm0 = xmm1[0],xmm0[0]
        vmovq   %rsi, %xmm1
        vmovq   %rdi, %xmm2
        vpunpcklqdq     %xmm1, %xmm2, %xmm1 # xmm1 = xmm2[0],xmm1[0]
        vinserti128     $1, %xmm0, %ymm1, %ymm0
        vpbroadcastq    .LCPI0_0(%rip), %ymm1 # ymm1 = [8192,8192,8192,8192]
        vpaddq  %ymm1, %ymm0, %ymm1
        leaq    8192(%r8), %rax
        leaq    8192(%r9), %r11
        vmovq   %r10, %xmm2
        vpbroadcastq    %xmm2, %ymm2
        vpbroadcastq    .LCPI0_1(%rip), %ymm3 # ymm3 = [9223372036854775808,9223372036854775808,9223372036854775808,9223372036854775808]
        vpxor   %ymm3, %ymm1, %ymm1
        vpxor   %ymm3, %ymm2, %ymm2
        cmpq    %r10, %rax
        seta    %al
        vpcmpgtq        %ymm2, %ymm1, %ymm1
        cmpq    %r8, %rbp
        seta    %bl
        cmpq    %r10, %r11
        seta    %r11b
        cmpq    %r9, %rbp
        vmovq   %rbp, %xmm2
        vpbroadcastq    %xmm2, %ymm2
        vpxor   %ymm3, %ymm0, %ymm0
        vpxor   %ymm3, %ymm2, %ymm2
        vpcmpgtq        %ymm0, %ymm2, %ymm0
        vpand   %ymm0, %ymm1, %ymm0
        vextracti128    $1, %ymm0, %xmm1
        seta    %r14b
        vpackssdw       %xmm1, %xmm0, %xmm2
        vpackssdw       %xmm0, %xmm1, %xmm0
        vpor    %xmm0, %xmm2, %xmm0
        vpshufd $229, %xmm0, %xmm1      # xmm1 = xmm0[1,1,2,3]
        vpor    %xmm1, %xmm0, %xmm0
        vpextrb $0, %xmm0, %ebp
        testb   $1, %bpl
        jne     .LBB0_5
# %bb.1:                                # %entry
        andb    %bl, %al
        jne     .LBB0_5
# %bb.2:                                # %entry
        andb    %r14b, %r11b
        jne     .LBB0_5
# %bb.3:                                # %vector.body.preheader
        xorl    %eax, %eax
        .p2align        4, 0x90
.LBB0_4:                                # %vector.body
                                        # =>This Inner Loop Header: Depth=1
        vmovdqu (%rsi,%rax), %ymm0
        vmovdqu 32(%rsi,%rax), %ymm1
        vmovdqu (%r9,%rax), %ymm2
        vmovdqu 32(%r9,%rax), %ymm3
        vpand   (%rdi,%rax), %ymm0, %ymm0
        vpand   32(%rdi,%rax), %ymm1, %ymm1
        vpand   (%rdx,%rax), %ymm0, %ymm0
        vpand   32(%rdx,%rax), %ymm1, %ymm1
        vpand   (%rcx,%rax), %ymm0, %ymm0
        vpand   32(%rcx,%rax), %ymm1, %ymm1
        vpxor   (%r8,%rax), %ymm2, %ymm2
        vpxor   32(%r8,%rax), %ymm3, %ymm3
        vpor    %ymm0, %ymm2, %ymm0
        vpor    %ymm1, %ymm3, %ymm1
        vmovdqu %ymm0, (%r10,%rax)
        vmovdqu %ymm1, 32(%r10,%rax)
        addq    $64, %rax
        cmpq    $8192, %rax             # imm = 0x2000
        jne     .LBB0_4
        jmp     .LBB0_7
.LBB0_5:                                # %loop.preheader
        xorl    %eax, %eax
        .p2align        4, 0x90
.LBB0_6:                                # %loop
                                        # =>This Inner Loop Header: Depth=1
        movq    (%rsi,%rax), %rbp
        movq    (%r9,%rax), %rbx
        andq    (%rdi,%rax), %rbp
        andq    (%rdx,%rax), %rbp
        andq    (%rcx,%rax), %rbp
        xorq    (%r8,%rax), %rbx
        orq     %rbp, %rbx
        movq    %rbx, (%r10,%rax)
        movq    8(%rsi,%rax), %rbp
        movq    8(%r9,%rax), %rbx
        andq    8(%rdi,%rax), %rbp
        andq    8(%rdx,%rax), %rbp
        andq    8(%rcx,%rax), %rbp
        xorq    8(%r8,%rax), %rbx
        orq     %rbp, %rbx
        movq    %rbx, 8(%r10,%rax)
        addq    $16, %rax
        cmpq    $8192, %rax             # imm = 0x2000
        jne     .LBB0_6
.LBB0_7:                                # %after_loop
        popq    %rbx
        popq    %r14
        popq    %rbp
        vzeroupper
        retq
.Lfunc_end0:
        .size   query, .Lfunc_end0-query
                                        # -- End function

        .section        ".note.GNU-stack","",@progbits

```
