; x86 assembly lexer showcase (NASM / Intel-style syntax)
; Covers directives, labels, instructions, registers, numbers, strings,
; operators, punctuation, and both common comment markers.

bits 64
default rel

section .rodata
message: db "hello, x86 assembly", 10, 0
message_len equ $ - message
hex_value: dq 0xDEADBEEF
decimal_value: dd 12345
binary_value: db 10101010b

section .text
global _start

; A regular label and a local label.
_start:
	mov rax, 1
	mov rdi, 1
	lea rsi, [rel message]
	mov rdx, message_len
	syscall

	; Arithmetic, comparison, and conditional branches.
	mov ecx, 10
.loop:
	sub ecx, 1
	cmp ecx, 0
	jne .loop

	; Addressing modes and common register families.
	mov r8d, dword [rbp - 4]
	add r8d, eax
	movdqa xmm0, xmm1

	; Punctuation and operators: ( ) [ ] { } , : + - * / = < > & | ^ ~ !
	mov byte [rdi + rax * 4], 0x7f
	ret

# GNU assembler style comment marker is recognized as a comment too.

# GNU assembler AT&T syntax examples.
.section .rodata
.Latt_message:
	.asciz "GNU assembler with AT&T operands"

.section .text
.globl att_demo
.type att_demo, @function
att_demo:
	pushq %rbp
	movq %rsp, %rbp
	movl $42, %eax
	addl $8, %eax
	leaq .Latt_message(%rip), %rdi
	cmpq $0, %rax
	je .Latt_done
	popq %rbp
	ret
.Latt_done:
	xorl %eax, %eax
	popq %rbp
	ret
.size att_demo, .-att_demo
