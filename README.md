# jitmap: Jitted bitmaps


# Language

A tiny expression language supporting the following C binary operators:
 - *!*: not
 - *&*: and
 - *|*: or
 - *^*: xor
Supported literals are:
 - *$[0-9]+*: index reference
 - *[A-Za-z0-9_]+*: named reference


## Examples
```
!a
a & b
($0 & ($1 | $2) ^ !$1)
```

# Generated assembly for query

```
$ ninja && tools/jitmap-ir '($0 & ($1 & ($2 & ($3 & $4))))'
ninja: no work to do.
; ModuleID = 'query'
source_filename = "($0 & ($1 & ($2 & ($3 & $4))))"

; Function Attrs: argmemonly
define void @query(i64* nocapture readonly %in, i64* nocapture readonly %in1, i64* nocapture readonly %in2, i64* nocapture readonly %in3, i64* nocapture readonly %in4, i64* nocapture %out) #0 {
entry:
  br label %loop

loop:                                             ; preds = %loop, %entry
  %i = phi i64 [ 0, %entry ], [ %next_i, %loop ]
  %gep_0 = getelementptr inbounds i64, i64* %in, i64 %i
  %bitcast_0 = bitcast i64* %gep_0 to <4 x i64>*
  %load_0 = load <4 x i64>, <4 x i64>* %bitcast_0
  %gep_1 = getelementptr inbounds i64, i64* %in1, i64 %i
  %bitcast_1 = bitcast i64* %gep_1 to <4 x i64>*
  %load_1 = load <4 x i64>, <4 x i64>* %bitcast_1
  %gep_2 = getelementptr inbounds i64, i64* %in2, i64 %i
  %bitcast_2 = bitcast i64* %gep_2 to <4 x i64>*
  %load_2 = load <4 x i64>, <4 x i64>* %bitcast_2
  %gep_3 = getelementptr inbounds i64, i64* %in3, i64 %i
  %bitcast_3 = bitcast i64* %gep_3 to <4 x i64>*
  %load_3 = load <4 x i64>, <4 x i64>* %bitcast_3
  %gep_4 = getelementptr inbounds i64, i64* %in4, i64 %i
  %bitcast_4 = bitcast i64* %gep_4 to <4 x i64>*
  %load_4 = load <4 x i64>, <4 x i64>* %bitcast_4
  %0 = and <4 x i64> %load_3, %load_4
  %1 = and <4 x i64> %load_2, %0
  %2 = and <4 x i64> %load_1, %1
  %3 = and <4 x i64> %load_0, %2
  %gep_output = getelementptr inbounds i64, i64* %out, i64 %i
  %bitcast_output = bitcast i64* %gep_output to <4 x i64>*
  store <4 x i64> %3, <4 x i64>* %bitcast_output
  %next_i = add i64 %i, 4
  %exit_cond = icmp eq i64 %next_i, 1024
  br i1 %exit_cond, label %after_loop, label %loop

after_loop:                                       ; preds = %loop
  ret void
}

attributes #0 = { argmemonly }
```

Use LLVM's opt and/or llc to transform the IR into native assembly.

```
tools/jitmap-ir '($0 & ($1 & ($2 & ($3 & $4))))' | llc-8 -O3 -mcpu=core-avx2
ninja: no work to do.
        .text
        .file   "($0 & ($1 & ($2 & ($3 & $4))))"
        .globl  query                   # -- Begin function query
        .p2align        4, 0x90
        .type   query,@function
query:                                  # @query
        .cfi_startproc
# %bb.0:                                # %entry
        xorl    %eax, %eax
        .p2align        4, 0x90
.LBB0_1:                                # %loop
                                        # =>This Inner Loop Header: Depth=1
        vmovaps (%rcx,%rax), %ymm0
        vandps  (%r8,%rax), %ymm0, %ymm0
        vandps  (%rdx,%rax), %ymm0, %ymm0
        vandps  (%rsi,%rax), %ymm0, %ymm0
        vandps  (%rdi,%rax), %ymm0, %ymm0
        vmovaps %ymm0, (%r9,%rax)
        addq    $32, %rax
        cmpq    $8192, %rax             # imm = 0x2000
        jne     .LBB0_1
# %bb.2:                                # %after_loop
        vzeroupper
        retq
.Lfunc_end0:
        .size   query, .Lfunc_end0-query
        .cfi_endproc
                                        # -- End function

        .section        ".note.GNU-stack","",@progbits
```

```
$ ninja && tools/jitmap-ir '$0 ^ $1' | opt-8 -S -O3 -mcpu=core-avx2 -mtriple=x86_64-pc-linux-gnu | llc-8 -O3 -mcpu=core-avx2
ninja: no work to do.
        .text
        .file   "($0 ^ $1)"
        .globl  query                   # -- Begin function query
        .p2align        4, 0x90
        .type   query,@function
query:                                  # @query
# %bb.0:                                # %entry
        xorl    %eax, %eax
        .p2align        4, 0x90
.LBB0_1:                                # %loop
                                        # =>This Inner Loop Header: Depth=1
        vmovaps (%rsi,%rax,8), %ymm0
        vxorps  (%rdi,%rax,8), %ymm0, %ymm0
        vmovaps %ymm0, (%rdx,%rax,8)
        vmovaps 32(%rsi,%rax,8), %ymm0
        vxorps  32(%rdi,%rax,8), %ymm0, %ymm0
        vmovaps %ymm0, 32(%rdx,%rax,8)
        vmovaps 64(%rsi,%rax,8), %ymm0
        vxorps  64(%rdi,%rax,8), %ymm0, %ymm0
        vmovaps %ymm0, 64(%rdx,%rax,8)
        vmovaps 96(%rsi,%rax,8), %ymm0
        vxorps  96(%rdi,%rax,8), %ymm0, %ymm0
        vmovaps %ymm0, 96(%rdx,%rax,8)
        vmovaps 128(%rsi,%rax,8), %ymm0
        vxorps  128(%rdi,%rax,8), %ymm0, %ymm0
        vmovaps %ymm0, 128(%rdx,%rax,8)
        vmovaps 160(%rsi,%rax,8), %ymm0
        vxorps  160(%rdi,%rax,8), %ymm0, %ymm0
        vmovaps %ymm0, 160(%rdx,%rax,8)
        vmovaps 192(%rsi,%rax,8), %ymm0
        vxorps  192(%rdi,%rax,8), %ymm0, %ymm0
        vmovaps %ymm0, 192(%rdx,%rax,8)
        vmovaps 224(%rsi,%rax,8), %ymm0
        vxorps  224(%rdi,%rax,8), %ymm0, %ymm0
        vmovaps %ymm0, 224(%rdx,%rax,8)
        addq    $32, %rax
        cmpq    $1024, %rax             # imm = 0x400
        jne     .LBB0_1
# %bb.2:                                # %after_loop
        vzeroupper
        retq
.Lfunc_end0:
        .size   query, .Lfunc_end0-query
                                        # -- End function

        .section        ".note.GNU-stack","",@progbits
```

