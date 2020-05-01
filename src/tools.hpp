#ifndef TOOLS_H
#define TOOLS_H

#include <cstdlib>
#include <string>

static std::string char2hex(char c) {
	std::string s = "\\x00";

	char d = c / 16;
	s[2] = d + (d < 10 ? '0' : 'a' - 10);
	d = c % 16;
	s[3] = d + (d < 10 ? '0' : 'a' - 10);

	return s;
}

static char hex2char(std::string s) {
	s[0] = '0';
	return std::strtol(&s[0], NULL, 16);
}

static int str2int(const char* s, uint base) {
	return std::strtol(s, NULL, base);
}

static double str2double(const char* s, uint base) {
	return std::stod(s);
}

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
