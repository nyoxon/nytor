// Plugin: x86 assembly syntax highlighting lexer
// Author: nyoxon

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>

#include "plugins/plugin.h"

struct asm_x86_lexer {
	struct lexer base;
};

static const char* const asm_x86_extensions[] = {
	".asm", ".s", ".S"
};

static const char asm_x86_comment[] = ";";

static const struct pair asm_x86_pairs[] = {
	{'(', ')', 1, 1},
	{'[', ']', 1, 1},
	{'{', '}', 1, 1},
	{'"', '"', 0, 1},
	{'\'', '\'', 0, 1}
};

static const char* const instructions[] = {
	"aaa", "aad", "aam", "aas", "adc", "add", "and", "arpl",
	"bound", "bsf", "bsr", "bswap", "bt", "btc", "btr", "bts",
	"call", "cbw", "cdq", "cdqe", "clc", "cld", "cli", "clts",
	"cmc", "cmova", "cmovae", "cmovb", "cmovbe", "cmovg", "cmovge",
	"cmovl", "cmovle", "cmovne", "cmovo", "cmovz", "cmp", "cmpsb",
	"cmpsd", "cmpsq", "cmpsw", "cmpxchg", "cpuid", "cwd", "cwde",
	"daa", "das", "dec", "div", "enter", "fadd", "fcom", "fdiv",
	"fild", "fistp", "fld", "fmul", "fninit", "fst", "fstp", "fsub",
	"hlt", "idiv", "imul", "in", "inc", "insb", "int", "int3",
	"invd", "iret", "ja", "jae", "jb", "jbe", "jc", "jcxz", "je",
	"jecxz", "jg", "jge", "jl", "jle", "jmp", "jna", "jnae", "jnb",
	"jnbe", "jnc", "jne", "jng", "jnge", "jnl", "jnle", "jno", "jnp",
	"jns", "jnz", "jo", "jp", "jpe", "jpo", "js", "jz", "lahf",
	"lar", "lea", "leave", "lfence", "lodsb", "lodsd", "lodsq", "lodsw",
	"loop", "loope", "loopne", "lsl", "mfence", "mov", "movabs", "movs",
	"movsb", "movsd", "movsq", "movsx", "movsxd", "movzx", "mul", "neg",
	"nop", "not", "or", "out", "outsb", "pop", "popa", "popad", "popf",
	"popfd", "popfq", "push", "pusha", "pushad", "pushf", "pushfd",
	"pushfq", "rcl", "rcr", "rdmsr", "rdtsc", "rep", "repe", "repne",
	"ret", "retf", "rol", "ror", "sal", "sar", "sbb", "scasb", "scasd",
	"scasq", "seta", "setae", "setb", "setbe", "sete", "setg", "setge",
	"setl", "setle", "setne", "setno", "setnp", "setns", "seto", "setp",
	"sets", "setz", "sfence", "shl", "shr", "stos", "sub", "syscall",
	"sysenter", "sysexit", "test", "ud2", "verr", "verw", "wait", "wbinvd",
	"xadd", "xchg", "xlat", "xor",
	"addps", "addss", "andps", "divps", "maxps", "minps", "movaps",
	"movd", "movdqa", "movdqu", "movhlps", "movhps", "movlhps", "movlps",
	"movss", "movups", "mulps", "orps", "paddb", "paddd", "paddq", "paddw",
	"pand", "pcmpeqb", "pcmpeqd", "pcmpgtd", "pmaddwd", "pmulhw", "pmullw",
	"por", "psubb", "psubd", "psubq", "psubw", "pxor", "shufps", "sqrtps",
	"subps", "ucomiss", "xorps"
};

static const char* const directives[] = {
	"align", "bits", "byte", "comm", "data", "extern", "global", "globl",
	"include", "int", "long", "section", "segment", "skip", "text", "type",
	"word", "zero", "ascii", "asciz", "byte", "double", "equ", "float",
	"hword", "long", "quad", "short", "space", "string", "uleb128", "value"
};

static const char* const registers[] = {
	"al", "ah", "ax", "eax", "rax", "bl", "bh", "bx", "ebx", "rbx",
	"cl", "ch", "cx", "ecx", "rcx", "dl", "dh", "dx", "edx", "rdx",
	"si", "esi", "rsi", "di", "edi", "rdi", "sp", "esp", "rsp",
	"bp", "ebp", "rbp", "ip", "eip", "rip", "cs", "ds", "es", "fs",
	"gs", "ss", "eflags", "rflags", "sil", "dil", "spl", "bpl",
	"xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
	"xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15",
	"ymm0", "ymm1", "ymm2", "ymm3", "ymm4", "ymm5", "ymm6", "ymm7",
	"ymm8", "ymm9", "ymm10", "ymm11", "ymm12", "ymm13", "ymm14", "ymm15",
	"zmm0", "zmm1", "zmm2", "zmm3", "zmm4", "zmm5", "zmm6", "zmm7",
	"zmm8", "zmm9", "zmm10", "zmm11", "zmm12", "zmm13", "zmm14", "zmm15",
	"mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7",
	"st", "st0", "st1", "st2", "st3", "st4", "st5", "st6", "st7"
};

static int is_space(uint32_t c) {
	return c <= 0xff && isspace((unsigned char) c) != 0;
}

static int is_identifier_start(uint32_t c) {
	return c == U'_' || c == U'.' || c == U'$' || c == U'%' ||
		(c < 0x80 && isalpha((unsigned char) c) != 0) || c >= 0x80;
}

static int is_identifier_continue(uint32_t c) {
	return c == U'_' || c == U'.' || c == U'$' || c == U'%' ||
		(c < 0x80 && isalnum((unsigned char) c) != 0) || c >= 0x80;
}

static int asm_x86_is_word_char(uint32_t c) {
	return is_identifier_continue(c);
}

static int word_equals
(
	const uint32_t* line,
	size_t begin,
	size_t end,
	const char* word
)
{
	size_t i = 0;
	while (word[i] != '\0') {
		if (begin + i >= end || line[begin + i] != (unsigned char) word[i]) {
			return 0;
		}
		i++;
	}
	return begin + i == end;
}

static int word_in_list
(
	const uint32_t* line,
	size_t begin,
	size_t end,
	const char* const* words,
	size_t count
)
{
	for (size_t i = 0; i < count; i++) {
		if (word_equals(line, begin, end, words[i])) return 1;
	}
	return 0;
}

static int emit
(
	struct token* tokens,
	size_t* count,
	size_t capacity,
	enum highlight highlight,
	size_t begin,
	size_t end
)
{
	if (begin == end) return 1;
	if (*count >= capacity) return 0;
	tokens[(*count)++] = (struct token) { highlight, begin, end };
	return 1;
}

static int is_hex(uint32_t c) {
	return (c >= U'0' && c <= U'9') ||
		(c >= U'a' && c <= U'f') || (c >= U'A' && c <= U'F');
}

static size_t scan_number(const uint32_t* line, size_t len, size_t i) {
	size_t p = i;
	if (line[p] == U'0' && p + 1 < len &&
		(line[p + 1] == U'x' || line[p + 1] == U'X'))
	{
		p += 2;
		while (p < len && (is_hex(line[p]) || line[p] == U'_')) p++;
		return p;
	}
	while (p < len && ((line[p] >= U'0' && line[p] <= U'9') ||
		line[p] == U'_' || line[p] == U'h' || line[p] == U'H' ||
		(line[p] >= U'a' && line[p] <= U'f') ||
		(line[p] >= U'A' && line[p] <= U'F'))) p++;
	if (p < len && (line[p] == U'b' || line[p] == U'B' ||
		line[p] == U'o' || line[p] == U'O' || line[p] == U'q' || line[p] == U'Q')) p++;
	return p;
}

static size_t asm_x86_tokenize_line
(
	struct lexer* lexer,
	const uint32_t* line,
	size_t len,
	struct token* tokens,
	size_t max_tokens,
	enum lex_state state_in,
	enum lex_state* state_out
)
{
	(void) lexer;
	(void) state_in;
	if (state_out) *state_out = LEX_STATE_NORMAL;
	if (!line || !tokens || max_tokens == 0) return 0;

	size_t count = 0;
	size_t i = 0;
	while (i < len && count < max_tokens) {
		if (is_space(line[i])) {
			i++;
			continue;
		}

		if (line[i] == U';' || line[i] == U'#') {
			emit(tokens, &count, max_tokens, HL_COMMENT, i, len);
			break;
		}

		if (line[i] == U'"' || line[i] == U'\'') {
			uint32_t quote = line[i];
			size_t begin = i++;
			while (i < len) {
				if (line[i] == U'\\' && i + 1 < len) {
					i += 2;
					continue;
				}
				if (line[i++] == quote) break;
			}
			emit(tokens, &count, max_tokens, HL_STRING, begin, i);
			continue;
		}

		if (line[i] >= U'0' && line[i] <= U'9') {
			size_t begin = i;
			i = scan_number(line, len, i);
			emit(tokens, &count, max_tokens, HL_NUMBER, begin, i);
			continue;
		}

		if (is_identifier_start(line[i])) {
			size_t begin = i++;
			while (i < len && is_identifier_continue(line[i])) i++;
			size_t next = i;
			while (next < len && is_space(line[next])) next++;

			enum highlight highlight = HL_NORMAL;
			if (line[i - 1] == U':' && i == begin + 1) {
				highlight = HL_FUNCTION;
			} else if (i < len && line[i] == U':') {
				highlight = HL_FUNCTION;
			} else if (line[begin] == U'.' || line[begin] == U'%') {
				highlight = HL_PREPROCESSOR;
			} else if (word_in_list(line, begin, i, registers, ARRAY_SIZE(registers))) {
				highlight = HL_TYPE;
			} else if (word_in_list(line, begin, i, instructions, ARRAY_SIZE(instructions))) {
				highlight = HL_KEYWORD;
			} else if (word_in_list(line, begin, i, directives, ARRAY_SIZE(directives))) {
				highlight = HL_SPECIFIER;
			} else if (next < len && line[next] == U'(') {
				highlight = HL_FUNCTION;
			}
			emit(tokens, &count, max_tokens, highlight, begin, i);
			continue;
		}

		if (line[i] == U'(' || line[i] == U')' || line[i] == U'[' ||
			line[i] == U']' || line[i] == U'{' || line[i] == U'}' ||
			line[i] == U',' || line[i] == U':' || line[i] == U'\'' ||
			line[i] == U'"')
		{
			emit(tokens, &count, max_tokens, HL_PUNCTUATION, i, i + 1);
		} else if (line[i] == U'+' || line[i] == U'-' || line[i] == U'*' ||
			line[i] == U'/' || line[i] == U'=' || line[i] == U'<' ||
			line[i] == U'>' || line[i] == U'&' || line[i] == U'|' ||
			line[i] == U'^' || line[i] == U'~' || line[i] == U'!')
		{
			emit(tokens, &count, max_tokens, HL_OPERATOR, i, i + 1);
		} else {
			emit(tokens, &count, max_tokens, HL_NORMAL, i, i + 1);
		}
		i++;
	}

	return count;
}

static void asm_x86_reset(struct lexer* lexer) {
	(void) lexer;
}

static void asm_x86_destroy(struct lexer* lexer) {
	free(lexer);
}

static struct lexer* asm_x86_create_lexer(void) {
	struct asm_x86_lexer* asm_x86 = calloc(1, sizeof(*asm_x86));
	if (!asm_x86) return NULL;

	asm_x86->base.tokenize_line = asm_x86_tokenize_line;
	asm_x86->base.destroy = asm_x86_destroy;
	asm_x86->base.reset = asm_x86_reset;
	return &asm_x86->base;
}

static const struct language_rules asm_x86_rules = {
	.pairs = asm_x86_pairs,
	.pair_count = ARRAY_SIZE(asm_x86_pairs),
	.comment_fmt = asm_x86_comment,
	.auto_indent = 1,
	.is_word_char = asm_x86_is_word_char
};

static const struct language_plugin asm_x86_plugin = {
	.name = "asm_x86",
	.extensions = asm_x86_extensions,
	.extension_count = ARRAY_SIZE(asm_x86_extensions),
	.reserved_filenames = NULL,
	.reserved_filename_count = 0,
	.create_lexer = asm_x86_create_lexer,
	.rules = &asm_x86_rules
};

const struct language_plugin* plugin_init(void) {
	return &asm_x86_plugin;
}
