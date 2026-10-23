#include <stdint.h>

#include "test.h"
#include "util/types/u32string.h"

int test_utf8_decoder(void) {
	uint32_t codepoint = 0;

	const char ascii[] = "A";
	CHECK(u32_decode(ascii, 1, &codepoint) == 1);
	CHECK(codepoint == U'A');

	const char beta[] = "\xce\xb2";
	CHECK(u32_decode(beta, sizeof(beta) - 1, &codepoint) == 2);
	CHECK(codepoint == 0x03B2);

	const char emoji[] = "\xf0\x9f\x98\x80";
	CHECK(u32_decode(emoji, sizeof(emoji) - 1, &codepoint) == 4);
	CHECK(codepoint == 0x1F600);

	const char bad_continuation[] = { (char) 0xC3, '(' };
	const char overlong[] = { (char) 0xC0, (char) 0xAF };
	const char surrogate[] = { (char) 0xED, (char) 0xA0, (char) 0x80 };
	const char too_large[] = {
		(char) 0xF4, (char) 0x90, (char) 0x80, (char) 0x80
	};
	const char truncated[] = { (char) 0xE2, (char) 0x82 };

	CHECK(u32_decode(bad_continuation, sizeof(bad_continuation), &codepoint) < 0);
	CHECK(u32_decode(overlong, sizeof(overlong), &codepoint) < 0);
	CHECK(u32_decode(surrogate, sizeof(surrogate), &codepoint) < 0);
	CHECK(u32_decode(too_large, sizeof(too_large), &codepoint) < 0);
	CHECK(u32_decode(truncated, sizeof(truncated), &codepoint) < 0);
	CHECK(u32_decode(NULL, 1, &codepoint) < 0);
	CHECK(u32_decode(ascii, 1, NULL) < 0);

	return 0;
}
