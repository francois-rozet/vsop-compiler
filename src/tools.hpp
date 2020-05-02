#ifndef TOOLS_H
#define TOOLS_H

#include <cstdlib>
#include <string>

/*
 * Convert a char into its "\xhh" hexadecimal representation
 *
 * @example char2hex('\n') -> "\\x0a"
 */
static std::string char2hex(char c) {
	std::string s = "\\x00";

	char d = c / 16;
	s[2] = d + (d < 10 ? '0' : 'a' - 10);
	d = c % 16;
	s[3] = d + (d < 10 ? '0' : 'a' - 10);

	return s;
}

/*
 * Convert a "\xhh" hexadecimal into its char representation
 *
 * @example hex2char("\\x0a") -> '\n'
 */
static char hex2char(std::string s) {
	s[0] = '0'; // -> "0xhh"
	return std::strtol(&s[0], NULL, 16);
}

/**
 * Convert a string to an int, given a base
 *
 * @example str2int("42", 10) -> 42
 * @example str2int("0x29a", 16) -> 666
 */
static int str2int(const char* s, uint base) {
	return std::strtol(s, NULL, base);
}

/**
 * Convert a string to an int, if possible
 *
 * @example str2maybeint("420") -> 420
 * @example str2maybeint("0x0aze") -> -1
 */
static int str2maybeint(const char* s) {
	size_t l = strlen(s);

	if (l > 2 and s[0] == '0' and s[1] == 'x') {
		for (size_t i = 2; i < l; ++i)
			if (s[i] >= '0' and s[i] <= '9');
			else if (s[i] >= 'a' and s[i] <= 'f');
			else if (s[i] >= 'A' and s[i] <= 'F');
			else return -1;
		return str2int(s, 16);
	}

	for (size_t i = 0; i < l; ++i)
		if (s[i] >= '0' and s[i] <= '9');
		else return -1;
	return str2int(s, 10);
}

/**
 * Convert a string to a double
 *
 * @example str2double("3.1415") -> 3.1415
 * @example str2double(".420") -> 0.42
 */
static double str2double(const char* s) {
	return std::stod(s);
}

/**
 * Convert a string to a double, if possible
 *
 * @example str2maybedouble("1.618") -> 1.618
 * @example str2maybedouble("0_0.") -> -1.
 */
static double str2maybedouble(const char* s) {
	size_t l = strlen(s);
	bool dot = false;

	for (size_t i = 0; i < l; ++i)
		if (s[i] >= '0' and s[i] <= '9');
		else if (s[i] == '.' and not dot)
			dot = true;
		else return -1.;

	return str2double(s);
}

/**
 * Convert an escape sequence into its associated char
 *
 * @example esc2char("\\n") -> '\n'
 * @example esc2char("\\x0a") -> '\n'
 */
static char esc2char(const char* s) {
	switch(s[1]) {
		case 'x': return hex2char(s);
		case 'b': return '\b';
		case 't': return '\t';
		case 'n': return '\n';
		case 'r': return '\r';
		default: return s[1];
	}
}

#endif
