#include "cursor.hpp"

using namespace std;

Cursor::Cursor(const vector<string>& input) : input_(input) {
	line_ = column_ = 0;

	lineNumber_ = input_.size();
	lineLength_ = input_[line_].length();

	this->update();
}

Cursor& Cursor::operator =(const Cursor& x) {
	line_ = x.line_;
	column_ = x.column_;
	lineLength_ = x.lineLength_;
	c_ = x.c_;

	return *this;
}

Cursor& Cursor::operator ++() {
	if (column_ == lineLength_) {
		if (line_ < lineNumber_ - 1) {
			column_ = 0;
			lineLength_ = input_[++line_].length();
		}
	} else
		++column_;

	this->update();

	return *this;
}

Cursor& Cursor::operator --() {
	if (column_ == 0) {
		if (line_ > 0) {
			lineLength_ = input_[--line_].length();
			column_ = lineLength_;
		}
	} else
		--column_;

	this->update();

	return *this;
}

bool Cursor::operator <(const Cursor& x) const {
	return line_ < x.line_ or (line_ == x.line_ and column_ < x.column_);
}

bool Cursor::operator ==(const Cursor& x) const {
	return line_ == x.line_ and column_ == x.column_;
}

bool Cursor::end_of_file() const {
	return c_ == '\f';
}

string Cursor::read_to(const Cursor& y) {
	string s;

	for (; *this < y; ++(*this))
		s += this->c_;

	return s;
}

char Cursor::update() {
	if (column_ == lineLength_)
		c_ = line_ == lineNumber_ - 1 ? '\f' : '\n';
	else
		c_ = input_[line_][column_];

	return c_;
}
