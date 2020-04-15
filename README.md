# jitmap: Jitted bitmaps

jitmap is a small library providing an execution engine for logical binary
expressions on bitmaps. Some examples where this is relevant:

* In search engines, posting lists (sorted sequences of integers) are encoded
  with bitmaps. Evaluating a search query (logical expression on
  keywords) can be implemented with logical expression on bitmaps.

* In columnar databases, selection vectors (index masks) are encoded with
  bitmaps, the results of predicate on column expressions. The bitmaps are then
  combined in a final bitmap.

* In stream processing systems with rule engines, e.g. adtech bid requests
  filtering with campaign rules, bitmaps are used as a first-pass optimization
  to lower the number of (costly) rules to evaluate on each incoming event.

jitmap compiles logical expressions into native functions with signature
`void fn(const char**, char*)`. The functions are optimized to minimize memory
transfers and uses the fastest vector instruction set provided by the host.

The following snippet shows an example of what jitmap achieves:

```C
typedef void (*dense_eval_fn)(const char**, char*);

// a, b, c, and output are pointers to bitmap
char* a, b, c, output;
// Note that for now, jitmap only supports static sized bitmaps.
const char** inputs[3] = {a, b, c};

// Compile an expression returned as a function pointer. The function can be
// called from any thread in the same address space and has a global lifetime.
// The generated symbol will be exposed to gdb and linux's perf utility.
const char* symbol_name = "a_and_b_and_c";
dense_eval_fn a_and_b_and_c = jitmap_compile(symbol_name, "a & b & c");

// The result of `a & b & c` will be stored in `output`, applied vertically
// using vectorized instruction available on the host.
a_and_b_and_c(inputs, output);
```

## Logical expression language

jitmap offers a small DSL language to evaluate bitwise operations on bitmaps.
The language supports variables (named bitmap), empty/full literals, and basic
operators: not `!`, and `&`, or `!`, xor `^`.

A query takes an expression and a list of bitmaps and execute the expression on
the bitmaps resulting in a new bitmap.

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

# 1 AND (a OR b) XOR c
($1 & (a | b) ^ c)
```

## Developing/Debugging

### *jitmap-ir* tool

The *jitmap-ir* command line utility takes an expression as first input argument
and dumps the generated LLVM' IR to stdout. It is useful to debug and peek at
the generated code. Using LLVM command line utilies, we can also look at the
expected generated assembly for any platform.

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

## Symbols with linux's perf

By default, perf will not be able to recognize the generated functions since the
symbols are not available statically. Luckily, perf has two mechanisms for jit
to register symbols. LLVM's jit use the jitdump [1] facility. At the time of
writing this, one needs to patch perf with [2], see commit 077a9b7bd1 for more
information.

```
# The `-k1` is required for jitdump to work.
$ perf record -k1 jitmap_benchmark

# By default, the output will be useless, since each instruction will be shown
# instead of grouped by symbols.
$ perf report --stdio
...
# Overhead  Command          Shared Object        Symbol
# ........  ...............  ...................  ....................................................................................
#
    29.09%  jitmap_benchmar  jitmap_benchmark     [.] jitmap::StaticBenchmark<jitmap::IntersectionFunctor<(jitmap::PopCountOption)1> >
    20.08%  jitmap_benchmar  jitmap_benchmark     [.] jitmap::StaticBenchmark<jitmap::IntersectionFunctor<(jitmap::PopCountOption)0> >
     1.78%  jitmap_benchmar  [JIT] tid 24013      [.] 0x00007f628c6cb045
     1.61%  jitmap_benchmar  [JIT] tid 24013      [.] 0x00007f628c6cb053
     1.59%  jitmap_benchmar  [JIT] tid 24013      [.] 0x00007f628c6cb197
     1.55%  jitmap_benchmar  [JIT] tid 24013      [.] 0x00007f628c6cb126
     1.51%  jitmap_benchmar  [JIT] tid 24013      [.] 0x00007f628c6cb035
     1.39%  jitmap_benchmar  [JIT] tid 24013      [.] 0x00007f628c6cb027

# We must process the generate perf.data file by injecting symbols name
$ perf inject --jit -i perf.data -o perf.jit.data && mv perf.jit.data perf.data
$ perf report --stdio
...
# Overhead  Command          Shared Object        Symbol
# ........  ...............  ...................  ....................................................................................
#
    29.09%  jitmap_benchmar  jitmap_benchmark     [.] jitmap::StaticBenchmark<jitmap::IntersectionFunctor<(jitmap::PopCountOption)1> >
    20.08%  jitmap_benchmar  jitmap_benchmark     [.] jitmap::StaticBenchmark<jitmap::IntersectionFunctor<(jitmap::PopCountOption)0> >
     6.48%  jitmap_benchmar  jitted-24013-16.so   [.] and_2_popcount
     6.46%  jitmap_benchmar  jitted-24013-32.so   [.] and_4_popcount
     6.42%  jitmap_benchmar  jitted-24013-46.so   [.] and_8_popcount
     6.19%  jitmap_benchmar  jitted-24013-77.so   [.] and_4
     6.19%  jitmap_benchmar  jitted-24013-61.so   [.] and_2
     4.59%  jitmap_benchmar  jitted-24013-91.so   [.] and_8
```

[1] https://elixir.bootlin.com/linux/v4.10/source/tools/perf/Documentation/jitdump-specification.txt

[2] https://lore.kernel.org/lkml/20191003105716.GB23291@krava/T/#u

# TODO

* Supports dynamic sized bitmaps
* Implement roaring-bitmap-like compressed bitmaps
* Get https://reviews.llvm.org/D67383 approved and merged to benefit from
  Tree-Height-Reduction pass.
* Provide a C front-end api.
