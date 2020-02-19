#include "cursor.hpp"

using namespace std;

Cursor::Cursor(const string& input) : input_(input) {
	inputLength_ = input_.length();
}

Cursor& Cursor::operator ++() {
	if (not *this)
		return *this;

	if (input_[i_++] == '\n') {
		++line_;
		column_ = 1;
	} else
		++column_;

	return *this;
}

Cursor& Cursor::operator =(const Cursor& x) {
	i_ = x.i_;
	line_ = x.line_;
	column_ = x.column_;

	return *this;
}
