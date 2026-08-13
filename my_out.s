	.file	"main_module"
	.text
	.globl	std_print_int_array_i32_p_i64   # -- Begin function std_print_int_array_i32_p_i64
	.p2align	4
	.type	std_print_int_array_i32_p_i64,@function
std_print_int_array_i32_p_i64:          # @std_print_int_array_i32_p_i64
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$32, %rsp
	movq	%rdi, -24(%rbp)
	movq	%rsi, -16(%rbp)
	movl	$0, -28(%rbp)
	movl	$91, %edi
	callq	putchar@PLT
	movq	$0, -8(%rbp)
	jmp	.LBB0_1
	.p2align	4
.LBB0_4:                                # %merge
                                        #   in Loop: Header=BB0_1 Depth=1
	incq	-8(%rbp)
.LBB0_1:                                # %while_cond
                                        # =>This Inner Loop Header: Depth=1
	movq	-8(%rbp), %rax
	cmpq	-16(%rbp), %rax
	jge	.LBB0_5
# %bb.2:                                # %while_body
                                        #   in Loop: Header=BB0_1 Depth=1
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movl	$0, -16(%rax)
	movq	-24(%rbp), %rax
	movq	-8(%rbp), %rcx
	movl	(%rax,%rcx,4), %esi
	movl	$.Lstr.0, %edi
	xorl	%eax, %eax
	callq	printf@PLT
	movq	-16(%rbp), %rax
	decq	%rax
	cmpq	%rax, -8(%rbp)
	jge	.LBB0_4
# %bb.3:                                # %then
                                        #   in Loop: Header=BB0_1 Depth=1
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movl	$0, -16(%rax)
	movl	$.Lstr.1, %edi
	movl	$.Lstr.2, %esi
	xorl	%eax, %eax
	callq	printf@PLT
	jmp	.LBB0_4
.LBB0_5:                                # %while_end
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movl	$0, -16(%rax)
	movl	$93, %edi
	callq	putchar@PLT
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end0:
	.size	std_print_int_array_i32_p_i64, .Lfunc_end0-std_print_int_array_i32_p_i64
	.cfi_endproc
                                        # -- End function
	.globl	std_print_uint_array_i32_p_i64  # -- Begin function std_print_uint_array_i32_p_i64
	.p2align	4
	.type	std_print_uint_array_i32_p_i64,@function
std_print_uint_array_i32_p_i64:         # @std_print_uint_array_i32_p_i64
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$32, %rsp
	movq	%rdi, -24(%rbp)
	movq	%rsi, -16(%rbp)
	movl	$0, -28(%rbp)
	movl	$91, %edi
	callq	putchar@PLT
	movq	$0, -8(%rbp)
	jmp	.LBB1_1
	.p2align	4
.LBB1_4:                                # %merge
                                        #   in Loop: Header=BB1_1 Depth=1
	incq	-8(%rbp)
.LBB1_1:                                # %while_cond
                                        # =>This Inner Loop Header: Depth=1
	movq	-8(%rbp), %rax
	cmpq	-16(%rbp), %rax
	jge	.LBB1_5
# %bb.2:                                # %while_body
                                        #   in Loop: Header=BB1_1 Depth=1
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movl	$0, -16(%rax)
	movq	-24(%rbp), %rax
	movq	-8(%rbp), %rcx
	movl	(%rax,%rcx,4), %esi
	movl	$.Lstr.3, %edi
	xorl	%eax, %eax
	callq	printf@PLT
	movq	-16(%rbp), %rax
	decq	%rax
	cmpq	%rax, -8(%rbp)
	jge	.LBB1_4
# %bb.3:                                # %then
                                        #   in Loop: Header=BB1_1 Depth=1
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movl	$0, -16(%rax)
	movl	$.Lstr.1, %edi
	movl	$.Lstr.2, %esi
	xorl	%eax, %eax
	callq	printf@PLT
	jmp	.LBB1_4
.LBB1_5:                                # %while_end
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movl	$0, -16(%rax)
	movl	$93, %edi
	callq	putchar@PLT
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end1:
	.size	std_print_uint_array_i32_p_i64, .Lfunc_end1-std_print_uint_array_i32_p_i64
	.cfi_endproc
                                        # -- End function
	.globl	std_print_long_array_void_p_i64 # -- Begin function std_print_long_array_void_p_i64
	.p2align	4
	.type	std_print_long_array_void_p_i64,@function
std_print_long_array_void_p_i64:        # @std_print_long_array_void_p_i64
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$32, %rsp
	movq	%rdi, -24(%rbp)
	movq	%rsi, -16(%rbp)
	movl	$0, -28(%rbp)
	movl	$91, %edi
	callq	putchar@PLT
	movq	$0, -8(%rbp)
	jmp	.LBB2_1
	.p2align	4
.LBB2_4:                                # %merge
                                        #   in Loop: Header=BB2_1 Depth=1
	incq	-8(%rbp)
.LBB2_1:                                # %while_cond
                                        # =>This Inner Loop Header: Depth=1
	movq	-8(%rbp), %rax
	cmpq	-16(%rbp), %rax
	jge	.LBB2_5
# %bb.2:                                # %while_body
                                        #   in Loop: Header=BB2_1 Depth=1
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movl	$0, -16(%rax)
	movq	-24(%rbp), %rax
	movq	-8(%rbp), %rcx
	movq	(%rax,%rcx,8), %rsi
	movl	$.Lstr.4, %edi
	xorl	%eax, %eax
	callq	printf@PLT
	movq	-16(%rbp), %rax
	decq	%rax
	cmpq	%rax, -8(%rbp)
	jge	.LBB2_4
# %bb.3:                                # %then
                                        #   in Loop: Header=BB2_1 Depth=1
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movl	$0, -16(%rax)
	movl	$.Lstr.1, %edi
	movl	$.Lstr.2, %esi
	xorl	%eax, %eax
	callq	printf@PLT
	jmp	.LBB2_4
.LBB2_5:                                # %while_end
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movl	$0, -16(%rax)
	movl	$93, %edi
	callq	putchar@PLT
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end2:
	.size	std_print_long_array_void_p_i64, .Lfunc_end2-std_print_long_array_void_p_i64
	.cfi_endproc
                                        # -- End function
	.globl	std_print_ulong_array_void_p_i64 # -- Begin function std_print_ulong_array_void_p_i64
	.p2align	4
	.type	std_print_ulong_array_void_p_i64,@function
std_print_ulong_array_void_p_i64:       # @std_print_ulong_array_void_p_i64
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$32, %rsp
	movq	%rdi, -24(%rbp)
	movq	%rsi, -16(%rbp)
	movl	$0, -28(%rbp)
	movl	$91, %edi
	callq	putchar@PLT
	movq	$0, -8(%rbp)
	jmp	.LBB3_1
	.p2align	4
.LBB3_4:                                # %merge
                                        #   in Loop: Header=BB3_1 Depth=1
	incq	-8(%rbp)
.LBB3_1:                                # %while_cond
                                        # =>This Inner Loop Header: Depth=1
	movq	-8(%rbp), %rax
	cmpq	-16(%rbp), %rax
	jge	.LBB3_5
# %bb.2:                                # %while_body
                                        #   in Loop: Header=BB3_1 Depth=1
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movl	$0, -16(%rax)
	movq	-24(%rbp), %rax
	movq	-8(%rbp), %rcx
	movq	(%rax,%rcx,8), %rsi
	movl	$.Lstr.5, %edi
	xorl	%eax, %eax
	callq	printf@PLT
	movq	-16(%rbp), %rax
	decq	%rax
	cmpq	%rax, -8(%rbp)
	jge	.LBB3_4
# %bb.3:                                # %then
                                        #   in Loop: Header=BB3_1 Depth=1
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movl	$0, -16(%rax)
	movl	$.Lstr.1, %edi
	movl	$.Lstr.2, %esi
	xorl	%eax, %eax
	callq	printf@PLT
	jmp	.LBB3_4
.LBB3_5:                                # %while_end
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movl	$0, -16(%rax)
	movl	$93, %edi
	callq	putchar@PLT
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end3:
	.size	std_print_ulong_array_void_p_i64, .Lfunc_end3-std_print_ulong_array_void_p_i64
	.cfi_endproc
                                        # -- End function
	.globl	std_print_single_array_void_p_i64 # -- Begin function std_print_single_array_void_p_i64
	.p2align	4
	.type	std_print_single_array_void_p_i64,@function
std_print_single_array_void_p_i64:      # @std_print_single_array_void_p_i64
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$32, %rsp
	movq	%rdi, -24(%rbp)
	movq	%rsi, -16(%rbp)
	movl	$0, -28(%rbp)
	movl	$91, %edi
	callq	putchar@PLT
	movq	$0, -8(%rbp)
	jmp	.LBB4_1
	.p2align	4
.LBB4_4:                                # %merge
                                        #   in Loop: Header=BB4_1 Depth=1
	incq	-8(%rbp)
.LBB4_1:                                # %while_cond
                                        # =>This Inner Loop Header: Depth=1
	movq	-8(%rbp), %rax
	cmpq	-16(%rbp), %rax
	jge	.LBB4_5
# %bb.2:                                # %while_body
                                        #   in Loop: Header=BB4_1 Depth=1
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movl	$0, -16(%rax)
	movq	-24(%rbp), %rax
	movq	-8(%rbp), %rcx
	movss	(%rax,%rcx,4), %xmm0            # xmm0 = mem[0],zero,zero,zero
	cvtss2sd	%xmm0, %xmm0
	movl	$.Lstr.6, %edi
	movb	$1, %al
	callq	printf@PLT
	movq	-16(%rbp), %rax
	decq	%rax
	cmpq	%rax, -8(%rbp)
	jge	.LBB4_4
# %bb.3:                                # %then
                                        #   in Loop: Header=BB4_1 Depth=1
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movl	$0, -16(%rax)
	movl	$.Lstr.1, %edi
	movl	$.Lstr.2, %esi
	xorl	%eax, %eax
	callq	printf@PLT
	jmp	.LBB4_4
.LBB4_5:                                # %while_end
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movl	$0, -16(%rax)
	movl	$93, %edi
	callq	putchar@PLT
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end4:
	.size	std_print_single_array_void_p_i64, .Lfunc_end4-std_print_single_array_void_p_i64
	.cfi_endproc
                                        # -- End function
	.globl	std_print_double_array_void_p_i64 # -- Begin function std_print_double_array_void_p_i64
	.p2align	4
	.type	std_print_double_array_void_p_i64,@function
std_print_double_array_void_p_i64:      # @std_print_double_array_void_p_i64
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$32, %rsp
	movq	%rdi, -24(%rbp)
	movq	%rsi, -16(%rbp)
	movl	$0, -28(%rbp)
	movl	$91, %edi
	callq	putchar@PLT
	movq	$0, -8(%rbp)
	jmp	.LBB5_1
	.p2align	4
.LBB5_4:                                # %merge
                                        #   in Loop: Header=BB5_1 Depth=1
	incq	-8(%rbp)
.LBB5_1:                                # %while_cond
                                        # =>This Inner Loop Header: Depth=1
	movq	-8(%rbp), %rax
	cmpq	-16(%rbp), %rax
	jge	.LBB5_5
# %bb.2:                                # %while_body
                                        #   in Loop: Header=BB5_1 Depth=1
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movl	$0, -16(%rax)
	movq	-24(%rbp), %rax
	movq	-8(%rbp), %rcx
	movsd	(%rax,%rcx,8), %xmm0            # xmm0 = mem[0],zero
	movl	$.Lstr.7, %edi
	movb	$1, %al
	callq	printf@PLT
	movq	-16(%rbp), %rax
	decq	%rax
	cmpq	%rax, -8(%rbp)
	jge	.LBB5_4
# %bb.3:                                # %then
                                        #   in Loop: Header=BB5_1 Depth=1
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movl	$0, -16(%rax)
	movl	$.Lstr.1, %edi
	movl	$.Lstr.2, %esi
	xorl	%eax, %eax
	callq	printf@PLT
	jmp	.LBB5_4
.LBB5_5:                                # %while_end
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movl	$0, -16(%rax)
	movl	$93, %edi
	callq	putchar@PLT
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end5:
	.size	std_print_double_array_void_p_i64, .Lfunc_end5-std_print_double_array_void_p_i64
	.cfi_endproc
                                        # -- End function
	.globl	main_hasError_i32               # -- Begin function main_hasError_i32
	.p2align	4
	.type	main_hasError_i32,@function
main_hasError_i32:                      # @main_hasError_i32
	.cfi_startproc
# %bb.0:                                # %entry
	movl	%edi, -4(%rsp)
	cmpl	$1, %edi
	jne	.LBB6_1
# %bb.4:                                # %then
	xorl	%eax, %eax
	movb	$1, %dl
	retq
.LBB6_1:                                # %merge
	cmpl	$2, -4(%rsp)
	jne	.LBB6_2
# %bb.5:                                # %then_2
	movl	$2, %eax
	retq
.LBB6_2:                                # %merge_2
	cmpl	$3, -4(%rsp)
	jne	.LBB6_3
# %bb.6:                                # %then_3
	movl	$3, %eax
	retq
.LBB6_3:                                # %merge_3
	movl	$4, %eax
	retq
.Lfunc_end6:
	.size	main_hasError_i32, .Lfunc_end6-main_hasError_i32
	.cfi_endproc
                                        # -- End function
	.globl	main                            # -- Begin function main
	.p2align	4
	.type	main,@function
main:                                   # @main
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	pushq	%rbx
	pushq	%rax
	.cfi_offset %rbx, -24
	xorl	%edi, %edi
	callq	main_hasError_i32@PLT
	movl	%eax, -16(%rbp)
	movl	%edx, %ecx
	andb	$1, %cl
	movb	%cl, -12(%rbp)
	testl	%eax, %eax
	sete	%al
	andb	%dl, %al
	cmpb	$1, %al
	jne	.LBB7_2
# %bb.1:                                # %then
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movl	$0, -16(%rax)
	movl	$.Lstr.1, %edi
	movl	$.Lstr.11, %esi
	xorl	%eax, %eax
	callq	printf@PLT
.LBB7_2:                                # %merge
	movl	$1, %edi
	callq	main_hasError_i32@PLT
	movq	%rsp, %rcx
	leaq	-16(%rcx), %rsp
	movl	%edx, %esi
	andb	$1, %sil
	movb	%sil, -12(%rcx)
	movl	%eax, -16(%rcx)
	testl	%eax, %eax
	sete	%al
	andb	%dl, %al
	cmpb	$1, %al
	jne	.LBB7_4
# %bb.3:                                # %then_2
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movl	$0, -16(%rax)
	movl	$.Lstr.1, %edi
	movl	$.Lstr.12, %esi
	xorl	%eax, %eax
	callq	printf@PLT
.LBB7_4:                                # %merge_2
	movl	$2, %edi
	callq	main_hasError_i32@PLT
	movq	%rsp, %rcx
	leaq	-16(%rcx), %rsp
	movl	%edx, %esi
	andb	$1, %sil
	movb	%sil, -12(%rcx)
	movl	%eax, -16(%rcx)
	xorl	%ebx, %ebx
	cmpl	$2, %eax
	sete	%cl
	cmovel	%ebx, %eax
	orb	%dl, %cl
	testl	%eax, %eax
	sete	%al
	andb	%cl, %al
	cmpb	$1, %al
	jne	.LBB7_6
# %bb.5:                                # %then_3
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movl	$0, -16(%rax)
	movl	$.Lstr.1, %edi
	movl	$.Lstr.12, %esi
	xorl	%eax, %eax
	callq	printf@PLT
.LBB7_6:                                # %merge_3
	movl	$3, %edi
	callq	main_hasError_i32@PLT
	movq	%rsp, %rcx
	leaq	-16(%rcx), %rsp
	movl	%edx, %esi
	andb	$1, %sil
	movb	%sil, -12(%rcx)
	movl	%eax, -16(%rcx)
	cmpl	$3, %eax
	setne	%cl
	cmovnel	%eax, %ebx
	andb	%dl, %cl
	testl	%ebx, %ebx
	setne	%al
	orb	%cl, %al
	cmpb	$1, %al
	jne	.LBB7_8
# %bb.7:                                # %then_4
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movl	$0, -16(%rax)
	movl	$.Lstr.1, %edi
	movl	$.Lstr.13, %esi
	xorl	%eax, %eax
	callq	printf@PLT
.LBB7_8:                                # %merge_4
	xorl	%eax, %eax
	leaq	-8(%rbp), %rsp
	popq	%rbx
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end7:
	.size	main, .Lfunc_end7-main
	.cfi_endproc
                                        # -- End function
	.type	.Lstr.13,@object                # @str.13
	.section	.rodata,"a",@progbits
	.p2align	4, 0x0
.Lstr.13:
	.asciz	"Shall not be printed!\n"
	.size	.Lstr.13, 23

	.type	.Lstr.12,@object                # @str.12
	.p2align	4, 0x0
.Lstr.12:
	.asciz	"Shall be printed\n"
	.size	.Lstr.12, 18

	.type	.Lstr.11,@object                # @str.11
	.p2align	4, 0x0
.Lstr.11:
	.asciz	"Shall not be printed!"
	.size	.Lstr.11, 22

	.type	.Lstr.10_data,@object           # @str.10_data
	.p2align	4, 0x0
.Lstr.10_data:
	.asciz	"purge: ErrNotOne\n"
	.size	.Lstr.10_data, 18

	.type	.Lstr.10,@object                # @str.10
	.p2align	3, 0x0
.Lstr.10:
	.long	17                              # 0x11
	.zero	4
	.quad	.Lstr.10_data
	.size	.Lstr.10, 16

	.type	.Lstr.9_data,@object            # @str.9_data
	.p2align	4, 0x0
.Lstr.9_data:
	.asciz	"purge: ErrThree\n"
	.size	.Lstr.9_data, 17

	.type	.Lstr.9,@object                 # @str.9
	.p2align	3, 0x0
.Lstr.9:
	.long	16                              # 0x10
	.zero	4
	.quad	.Lstr.9_data
	.size	.Lstr.9, 16

	.type	.Lstr.8_data,@object            # @str.8_data
.Lstr.8_data:
	.asciz	"purge: ErrTwo\n"
	.size	.Lstr.8_data, 15

	.type	.Lstr.8,@object                 # @str.8
	.p2align	3, 0x0
.Lstr.8:
	.long	14                              # 0xe
	.zero	4
	.quad	.Lstr.8_data
	.size	.Lstr.8, 16

	.type	.Lstr.7,@object                 # @str.7
.Lstr.7:
	.asciz	"%lf"
	.size	.Lstr.7, 4

	.type	.Lstr.6,@object                 # @str.6
.Lstr.6:
	.asciz	"%f"
	.size	.Lstr.6, 3

	.type	.Lstr.5,@object                 # @str.5
.Lstr.5:
	.asciz	"%lu"
	.size	.Lstr.5, 4

	.type	.Lstr.4,@object                 # @str.4
.Lstr.4:
	.asciz	"%ld"
	.size	.Lstr.4, 4

	.type	.Lstr.3,@object                 # @str.3
.Lstr.3:
	.asciz	"%u"
	.size	.Lstr.3, 3

	.type	.Lstr.2,@object                 # @str.2
.Lstr.2:
	.asciz	", "
	.size	.Lstr.2, 3

	.type	.Lstr.1,@object                 # @str.1
.Lstr.1:
	.asciz	"%s"
	.size	.Lstr.1, 3

	.type	.Lstr.0,@object                 # @str.0
.Lstr.0:
	.asciz	"%d"
	.size	.Lstr.0, 3

	.section	".note.GNU-stack","",@progbits
