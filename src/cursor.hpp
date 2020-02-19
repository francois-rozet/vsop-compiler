#ifndef CURSOR_H
#define CURSOR_H

#include <string>

class Cursor {
	public:
		/* Constructors */
		Cursor(const std::string&);

		/* Operators */
		Cursor& operator ++();
		Cursor& operator =(const Cursor&);

		bool operator <(const Cursor& x) const { return i_ < x.i_; };
		bool operator <=(const Cursor& x) const { return i_ <= x.i_; };
		bool operator ==(const Cursor& x) const { return i_ == x.i_; };

		const char& operator *() const { return input_[i_]; };
		operator bool() const { return i_ != inputLength_; };

		/* Accessors */
		unsigned line() const { return line_; }
		unsigned column() const { return column_; }

	private:
		/* Variables */
		const std::string& input_;
		unsigned i_ = 0, line_ = 1, column_ = 1, inputLength_;
};

static std::string read(Cursor& x, const Cursor& y) {
	std::string s;

	for (; x < y; ++x)
		s += *x;

	return s;
}

#endif
