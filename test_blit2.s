.text
.balign 16
.globl main
main:
	endbr64
	subq $16, %rsp
	movl $42, 0(%rsp)
	movl $42, %eax
	addq $16, %rsp
	ret
.type main, @function
.size main, .-main
/* end function main */

.section .note.GNU-stack,"",@progbits
