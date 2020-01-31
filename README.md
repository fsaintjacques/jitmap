# jitmap: Jitted bitmaps

jitmap is a small library providing an execution engine evaluating logical
binary expressions on bitmaps. Some examples:

* In search engines, posting lists are often represented by sequence of
  integers, or bitmaps. Evaluating a search term (logical expression on
  keywords) can be implemented by a logical expression on bitmaps.

* In columnar databases, bitmaps are used to represent selection vectors,
  themselves the intermediary representation of the results of predicate filter
  on rows. The bitmaps are then combined in a final bitmap, the predicate
  expression transformed in a logical expression on bitmaps.

* In stream system with rule engines, e.g. adtech bid filtering on campaign
  rules, bitmaps are used as a first-pass optimization to lower the number of
  campaigns' rules to evaluate on each incoming event.

jitmap compiles logical expresion into native functions that takes multiple
inputs dense bitmaps pointers, e.g. `const char**`, and write the result in a
destination pointer, e.g. `const char*`. The signature of such generated
functions are `void fn(const char**, char*)`. The functions are then callable
in the same address space.

The following snippet shows an example of what jitmap achieves:

```C
typedef void (*dense_eval_fn)(const char**, char*);

dense_eval_fn a_and_b = jitmap_compile("a_and_b", "a ^ b");
const char** addrs[2] = {&a, &b};

// `output` will now contains the result of `a & b` applied vertically likely
// using // vectorized instruction available on the host running this code, as
// opposed to where the code was compiled.
a_and_b(addrs, &output);
```

## Logical expression language

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

The *jitmap-ir* binary takes an expression as first input argument and dumps the
generated LLVM' IR to stdout. It is useful to debug and peek at the generated
code. Using LLVM command line utilies, we can also look at the expected
generated assembly for any platform.

```llvm
# tools/jitmap-ir "(a & b) | (c & c) | (c ^ d) | (c & b) | (d ^ a)"
; ModuleID = 'jitmap_ir'
source_filename = "jitmap_ir"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: argmemonly
define void @query(i32** nocapture readonly %inputs, i32* nocapture %output) #0 {
entry:
  %bitmap_gep_0 = getelementptr inbounds i32*, i32** %inputs, i64 0
  %bitmap_0 = load i32*, i32** %bitmap_gep_0
  %bitmap_gep_1 = getelementptr inbounds i32*, i32** %inputs, i64 1
  %bitmap_1 = load i32*, i32** %bitmap_gep_1
  %bitmap_gep_2 = getelementptr inbounds i32*, i32** %inputs, i64 2
  %bitmap_2 = load i32*, i32** %bitmap_gep_2
  %bitmap_gep_3 = getelementptr inbounds i32*, i32** %inputs, i64 3
  %bitmap_3 = load i32*, i32** %bitmap_gep_3
  br label %loop

loop:                                             ; preds = %loop, %entry
  %i = phi i64 [ 0, %entry ], [ %next_i, %loop ]
  %gep_0 = getelementptr inbounds i32, i32* %bitmap_0, i64 %i
  %load_0 = load i32, i32* %gep_0
  %gep_1 = getelementptr inbounds i32, i32* %bitmap_1, i64 %i
  %load_1 = load i32, i32* %gep_1
  %gep_2 = getelementptr inbounds i32, i32* %bitmap_2, i64 %i
  %load_2 = load i32, i32* %gep_2
  %gep_3 = getelementptr inbounds i32, i32* %bitmap_3, i64 %i
  %load_3 = load i32, i32* %gep_3
  %0 = and i32 %load_0, %load_1
  %1 = and i32 %load_2, %load_2
  %2 = or i32 %0, %1
  %3 = xor i32 %load_2, %load_3
  %4 = or i32 %2, %3
  %5 = and i32 %load_2, %load_1
  %6 = or i32 %4, %5
  %7 = xor i32 %load_3, %load_0
  %8 = or i32 %6, %7
  %gep_output = getelementptr inbounds i32, i32* %output, i64 %i
  store i32 %8, i32* %gep_output
  %next_i = add i64 %i, 1
  %exit_cond = icmp eq i64 %next_i, 2048
  br i1 %exit_cond, label %after_loop, label %loop

after_loop:                                       ; preds = %loop
  ret void
}

attributes #0 = { argmemonly }
```

We can then use LLVM's `opt` and `llc` to transform the IR into native assembly.

```objdump
# tools/jitmap-ir "(a & b) | (c & c) | (c ^ d) | (c & b) | (d ^ a)" | llc -O3
        .text
        .file   "jitmap_ir"
        .globl  query                   # -- Begin function query
        .p2align        4, 0x90
        .type   query,@function
query:                                  # @query
        .cfi_startproc
# %bb.0:                                # %entry
        pushq   %rbp
        .cfi_def_cfa_offset 16
        pushq   %rbx
        .cfi_def_cfa_offset 24
        .cfi_offset %rbx, -24
        .cfi_offset %rbp, -16
        movq    (%rdi), %r8
        movq    8(%rdi), %r9
        movq    16(%rdi), %r10
        movq    24(%rdi), %r11
        movq    $-8192, %rax            # imm = 0xE000
        .p2align        4, 0x90
.LBB0_1:                                # %loop
                                        # =>This Inner Loop Header: Depth=1
        movl    8192(%r8,%rax), %ecx
        movl    8192(%r9,%rax), %edx
        movl    8192(%r10,%rax), %edi
        movl    8192(%r11,%rax), %ebx
        movl    %edi, %ebp
        xorl    %ebx, %ebp
        xorl    %ecx, %ebx
        andl    %edx, %ecx
        orl     %edi, %ebp
        andl    %edx, %edi
        orl     %ebp, %edi
        orl     %edi, %ebx
        orl     %ecx, %ebx
        movl    %ebx, 8192(%rsi,%rax)
        addq    $4, %rax
        jne     .LBB0_1
# %bb.2:                                # %after_loop
        popq    %rbx
        .cfi_def_cfa_offset 16
        popq    %rbp
        .cfi_def_cfa_offset 8
        retq
.Lfunc_end0:
        .size   query, .Lfunc_end0-query
        .cfi_endproc
                                        # -- End function

        .section        ".note.GNU-stack","",@progbits

```

This code is still not fully optimized, `opt` is used for this.

```objdump
# tools/jitmap-ir "(a & b) | (c & c) | (c ^ d) | (c & b) | (d ^ a)" | opt -O3 -S -mcpu=core-avx2| llc -O3
ninja: no work to do.
        .text
        .file   "jitmap_ir"
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
        pushq   %r15
        pushq   %r14
        pushq   %r12
        pushq   %rbx
#  ...
# And the holy grail fully vectorized loop
.LBB0_2:                                # %vector.body
                                        # =>This Inner Loop Header: Depth=1
        vmovdqu (%r14,%rbx), %ymm0
        vmovdqu 32(%r14,%rbx), %ymm1
        vmovdqu (%r12,%rbx), %ymm2
        vmovdqu 32(%r12,%rbx), %ymm3
        vmovdqu (%rdi,%rbx), %ymm4
        vmovdqu 32(%rdi,%rbx), %ymm5
        vpand   (%r15,%rbx), %ymm0, %ymm6
        vpand   32(%r15,%rbx), %ymm1, %ymm7
        vpor    %ymm2, %ymm6, %ymm6
        vpor    %ymm3, %ymm7, %ymm7
        vpxor   %ymm2, %ymm4, %ymm2
        vpxor   %ymm3, %ymm5, %ymm3
        vpxor   %ymm0, %ymm4, %ymm0
        vpor    %ymm0, %ymm2, %ymm0
        vpor    %ymm0, %ymm6, %ymm0
        vpxor   %ymm1, %ymm5, %ymm1
        vpor    %ymm1, %ymm3, %ymm1
        vpor    %ymm1, %ymm7, %ymm1
        vmovdqu %ymm0, (%rsi,%rbx)
        vmovdqu %ymm1, 32(%rsi,%rbx)
        addq    $64, %rbx
        cmpq    $8192, %rbx             # imm = 0x2000
        jne     .LBB0_2
.LBB0_5:                                # %after_loop
        popq    %rbx
        popq    %r12
        popq    %r14
        popq    %r15
        popq    %rbp
                vzeroupper
        retq
.Lfunc_end0:
        .size   query, .Lfunc_end0-query
                                        # -- End function

        .section        ".note.GNU-stack","",@progbits
```
